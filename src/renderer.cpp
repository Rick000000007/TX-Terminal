#include <unistd.h>
#include "tx/renderer.hpp"
#include <cmath>

#if defined(TX_ANDROID)
    #include <GLES3/gl3.h>
#else
    #include <GL/gl.h>
#endif

namespace tx {

// Shader sources
const char* Renderer::vertex_shader_source_ = R"(
#version 300 es
precision highp float;

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texcoord;

uniform vec2 u_cell_size;
uniform vec2 u_atlas_size;
uniform mat4 u_projection;

out vec2 v_texcoord;
flat out int v_instance_id;

struct CellData {
    vec4 position;  // x, y, u, v
    vec4 size;      // w, h, glyph_index, flags
    vec4 colors;    // fg_rgba, bg_rgba
};

layout(std140) uniform CellBuffer {
    CellData cells[4096];
};

void main() {
    v_instance_id = gl_InstanceID;
    CellData cell = cells[gl_InstanceID];
    
    vec2 pos = a_position * cell.size.xy + cell.position.xy;
    gl_Position = u_projection * vec4(pos, 0.0, 1.0);
    
    v_texcoord = a_texcoord * cell.size.xy + cell.position.zw;
    v_texcoord /= u_atlas_size;
}
)";

const char* Renderer::fragment_shader_source_ = R"(
#version 300 es
precision highp float;

in vec2 v_texcoord;
flat in int v_instance_id;

uniform sampler2D u_atlas;

out vec4 frag_color;

void main() {
    float alpha = texture(u_atlas, v_texcoord).r;
    frag_color = vec4(1.0, 1.0, 1.0, alpha);
}
)";

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int viewport_width, int viewport_height) {
    viewport_width_ = viewport_width;
    viewport_height_ = viewport_height;
    
    // Create shader program
    if (!createShaderProgram()) {
        return false;
    }
    
    // Create buffers
    if (!createBuffers()) {
        return false;
    }
    
    // Initialize font atlas
    font_atlas_ = std::make_unique<FontAtlas>();
    if (!font_atlas_->initialize(config_.font_size)) {
        return false;
    }
    
    // Update config with actual cell size
    config_.cell_width = font_atlas_->getCellWidth();
    config_.cell_height = font_atlas_->getCellHeight();
    
    // Create glyph texture
    updateGlyphTexture();
    
    // Set initial viewport
    setViewport(viewport_width, viewport_height);
    
    return true;
}

void Renderer::shutdown() {
    if (shader_program_) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ibo_) {
        glDeleteBuffers(1, &ibo_);
        ibo_ = 0;
    }
    if (glyph_texture_) {
        glDeleteTextures(1, &glyph_texture_);
        glyph_texture_ = 0;
    }
    if (ubo_) {
        glDeleteBuffers(1, &ubo_);
        ubo_ = 0;
    }
    
    if (font_atlas_) {
        font_atlas_->shutdown();
        font_atlas_.reset();
    }
}

bool Renderer::loadFont(const std::string& path, float size) {
    config_.font_path = path;
    config_.font_size = size;
    
    if (font_atlas_) {
        font_atlas_->shutdown();
        if (!font_atlas_->initialize(size)) {
            return false;
        }
        updateGlyphTexture();
    }
    
    return true;
}

bool Renderer::loadSystemFont() {
    // Try common system font paths
    const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/system/fonts/DroidSansMono.ttf",  // Android
        "/system/fonts/RobotoMono-Regular.ttf",  // Android
        nullptr
    };
    
    for (int i = 0; paths[i]; ++i) {
        if (access(paths[i], R_OK) == 0) {
            return loadFont(paths[i], config_.font_size);
        }
    }
    
    return false;
}

void Renderer::render(const Screen& screen) {
    if (!shader_program_) return;
    
    metrics_.draw_calls = 0;
    metrics_.glyphs_rendered = 0;
    
    // Clear background
    float bg_r = ((config_.background_color >> 16) & 0xFF) / 255.0f;
    float bg_g = ((config_.background_color >> 8) & 0xFF) / 255.0f;
    float bg_b = (config_.background_color & 0xFF) / 255.0f;
    glClearColor(bg_r, bg_g, bg_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Build cell data
    buildCellData(screen);
    
    if (cell_buffer_.empty()) return;
    
    // Upload cell data
    uploadCells(cell_buffer_);
    
    // Bind shader and uniforms
    glUseProgram(shader_program_);
    
    glUniform2f(u_cell_size_, config_.cell_width, config_.cell_height);
    glUniform2f(u_atlas_size_, 
                static_cast<float>(font_atlas_->getTextureWidth()),
                static_cast<float>(font_atlas_->getTextureHeight()));
    
    // Set up projection matrix (orthographic)
    float projection[16] = {
        2.0f / viewport_width_, 0, 0, 0,
        0, -2.0f / viewport_height_, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    };
    glUniformMatrix4fv(u_projection_, 1, GL_FALSE, projection);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glyph_texture_);
    glUniform1i(glGetUniformLocation(shader_program_, "u_atlas"), 0);
    
    // Bind UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_);
    GLuint block_index = glGetUniformBlockIndex(shader_program_, "CellBuffer");
    glUniformBlockBinding(shader_program_, block_index, 0);
    
    // Bind VAO and draw
    glBindVertexArray(vao_);
    
    GLsizei instance_count = static_cast<GLsizei>(cell_buffer_.size());
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, instance_count);
    
    metrics_.draw_calls++;
    metrics_.glyphs_rendered = instance_count;
    
    // Unbind
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::setViewport(int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;
    glViewport(0, 0, width, height);
}

void Renderer::clear() {
    glClear(GL_COLOR_BUFFER_BIT);
}

bool Renderer::createShaderProgram() {
    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_shader_source_);
    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_shader_source_);
    
    if (!vertex_shader || !fragment_shader) {
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        return false;
    }
    
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertex_shader);
    glAttachShader(shader_program_, fragment_shader);
    glLinkProgram(shader_program_);
    
    GLint linked;
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(shader_program_, sizeof(log), nullptr, log);
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    // Get uniform locations
    u_projection_ = glGetUniformLocation(shader_program_, "u_projection");
    u_cell_size_ = glGetUniformLocation(shader_program_, "u_cell_size");
    u_atlas_size_ = glGetUniformLocation(shader_program_, "u_atlas_size");
    
    return true;
}

bool Renderer::createBuffers() {
    // Create VAO
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    
    // Quad vertices (position + texcoord)
    float vertices[] = {
        // Position    // Texcoord
        0.0f, 0.0f,   0.0f, 0.0f,  // Top-left
        1.0f, 0.0f,   1.0f, 0.0f,  // Top-right
        1.0f, 1.0f,   1.0f, 1.0f,  // Bottom-right
        0.0f, 1.0f,   0.0f, 1.0f   // Bottom-left
    };
    
    // Create VBO
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 
                          reinterpret_cast<void*>(2 * sizeof(float)));
    
    // Create IBO (indices for two triangles)
    unsigned short indices[] = {0, 1, 2, 0, 2, 3};
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Create UBO for cell data
    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferData(GL_UNIFORM_BUFFER, 4096 * sizeof(GPUCell), nullptr, GL_DYNAMIC_DRAW);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    return true;
}

void Renderer::updateGlyphTexture() {
    if (!font_atlas_) return;
    
    if (glyph_texture_) {
        glDeleteTextures(1, &glyph_texture_);
    }
    
    glGenTextures(1, &glyph_texture_);
    glBindTexture(GL_TEXTURE_2D, glyph_texture_);
    
    // Initialize with empty texture
    int width = font_atlas_->getTextureWidth();
    int height = font_atlas_->getTextureHeight();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::uploadCells(const std::vector<GPUCell>& cells) {
    if (cells.empty()) return;
    
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, cells.size() * sizeof(GPUCell), cells.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

GLuint Renderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

void Renderer::buildCellData(const Screen& screen) {
    cell_buffer_.clear();

    int cols = screen.cols();
    int rows = screen.rows();

    // Get cursor position and blink state
    int cursor_col = screen.cursorCol();
    int cursor_row = screen.cursorRow();
    bool cursor_visible = screen.cursorVisible();
    bool cursor_blink_state = screen.cursorBlinkState();
    int cursor_style = screen.cursorStyle();
    Color cursor_color = screen.cursorColor();

    for (int row = 0; row < rows; ++row) {
        const Cell* line = screen.getRow(row);
        if (!line) continue;

        for (int col = 0; col < cols; ++col) {
            const Cell& cell = line[col];

            // Check if this is the cursor position
            bool is_cursor = (col == cursor_col && row == cursor_row && cursor_visible && cursor_blink_state);

            if (cell.empty() && !is_cursor) continue;

            const GlyphInfo* glyph = nullptr;
            if (!cell.empty()) {
                glyph = font_atlas_->getGlyph(cell.codepoint,
                                                cell.attrs.bold,
                                                cell.attrs.italic);
            }

            GPUCell gpu_cell;
            gpu_cell.x = col * config_.cell_width;
            gpu_cell.y = row * config_.cell_height;

            if (glyph) {
                gpu_cell.u = glyph->u0 * font_atlas_->getTextureWidth();
                gpu_cell.v = glyph->v0 * font_atlas_->getTextureHeight();
                gpu_cell.w = (glyph->u1 - glyph->u0) * font_atlas_->getTextureWidth();
                gpu_cell.h = (glyph->v1 - glyph->v0) * font_atlas_->getTextureHeight();
            } else {
                gpu_cell.u = gpu_cell.v = 0;
                gpu_cell.w = gpu_cell.h = 0;
            }

            // Handle cursor rendering
            if (is_cursor) {
                // Cursor is visible - modify colors based on cursor style
                switch (cursor_style) {
                    case 0:  // Block cursor - invert colors
                        gpu_cell.fg_color = cell.empty() ? cursor_color : cell.attrs.bg_color;
                        gpu_cell.bg_color = cursor_color;
                        break;
                    case 1:  // Underline cursor
                        gpu_cell.fg_color = cell.empty() ? config_.background_color : cell.attrs.fg_color;
                        gpu_cell.bg_color = cell.empty() ? config_.background_color : cell.attrs.bg_color;
                        // Add underline flag
                        gpu_cell.flags = 4;  // Underline
                        break;
                    case 2:  // Bar cursor
                        gpu_cell.fg_color = cell.empty() ? config_.background_color : cell.attrs.fg_color;
                        gpu_cell.bg_color = cell.empty() ? config_.background_color : cell.attrs.bg_color;
                        // Bar is rendered as a narrow block at the left edge
                        gpu_cell.w = 2.0f;  // 2 pixel wide bar
                        break;
                    default:
                        gpu_cell.fg_color = cell.attrs.fg_color;
                        gpu_cell.bg_color = cell.attrs.bg_color;
                        break;
                }
            } else {
                gpu_cell.fg_color = cell.attrs.fg_color;
                gpu_cell.bg_color = cell.attrs.bg_color;
            }

            gpu_cell.glyph_index = 0;  // Not used with texture atlas
            gpu_cell.flags = (cell.attrs.bold ? 1 : 0) |
                            (cell.attrs.italic ? 2 : 0) |
                            (cell.attrs.underline ? 4 : 0);

            cell_buffer_.push_back(gpu_cell);
        }
    }
}

// FontAtlas implementation
FontAtlas::FontAtlas() = default;

FontAtlas::~FontAtlas() {
    shutdown();
}

bool FontAtlas::initialize(float size) {
    font_size_ = size;
    
    // Initialize FreeType (platform-specific)
    // This is a stub - actual implementation would use FreeType
    
    // Calculate metrics (placeholder)
    cell_width_ = size * 0.6f;
    cell_height_ = size * 1.2f;
    ascent_ = size * 0.8f;
    descent_ = size * 0.2f;
    line_gap_ = size * 0.1f;
    
    // Initialize root node for atlas packing
    nodes_.push_back({0, 0, texture_width_});
    
    return true;
}

void FontAtlas::shutdown() {
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    nodes_.clear();
    glyphs_.clear();
}

const GlyphInfo* FontAtlas::getGlyph(char32_t codepoint, bool bold, bool italic) {
    uint64_t key = (static_cast<uint64_t>(codepoint) << 16) | 
                   (bold ? 0x100 : 0) | (italic ? 0x200 : 0);
    
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) {
        return &it->second;
    }
    
    // Render new glyph
    GlyphInfo info;
    if (renderGlyph(codepoint, bold, italic, info)) {
        glyphs_[key] = info;
        return &glyphs_[key];
    }
    
    return nullptr;
}

bool FontAtlas::packGlyph(int width, int height, int& x, int& y) {
    // Simple shelf packing algorithm
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].y + height <= texture_height_ && 
            nodes_[i].x + width <= nodes_[i].width) {
            x = nodes_[i].x;
            y = nodes_[i].y;
            nodes_[i].x += width;
            return true;
        }
    }
    
    // Try to create new shelf
    int shelf_y = 0;
    for (const auto& node : nodes_) {
        shelf_y = std::max(shelf_y, node.y + height);
    }
    
    if (shelf_y + height <= texture_height_) {
        x = 0;
        y = shelf_y;
        nodes_.push_back({width, shelf_y, texture_width_});
        return true;
    }
    
    return false;  // Atlas full
}

bool FontAtlas::renderGlyph(char32_t codepoint, bool bold, bool italic, GlyphInfo& info) {
    // Placeholder - actual implementation would:
    // 1. Load glyph from FreeType
    // 2. Render to bitmap
    // 3. Pack into atlas texture
    // 4. Update texture with glTexSubImage2D
    
    int glyph_width = static_cast<int>(cell_width_);
    int glyph_height = static_cast<int>(cell_height_);
    
    int x, y;
    if (!packGlyph(glyph_width, glyph_height, x, y)) {
        return false;
    }
    
    // Calculate texture coordinates
    info.u0 = static_cast<float>(x) / texture_width_;
    info.v0 = static_cast<float>(y) / texture_height_;
    info.u1 = static_cast<float>(x + glyph_width) / texture_width_;
    info.v1 = static_cast<float>(y + glyph_height) / texture_height_;
    
    info.x0 = 0;
    info.y0 = -descent_;
    info.x1 = glyph_width;
    info.y1 = glyph_height;
    info.advance = cell_width_;
    
    return true;
}

} // namespace tx
