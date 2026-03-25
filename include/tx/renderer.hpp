#pragma once

#include "common.hpp"
#include "screen.hpp"

#if defined(TX_ANDROID)
    #include <GLES3/gl3.h>
#else
    #include <GL/gl.h>
#endif

#include <unordered_map>
#include <memory>

namespace tx {

// Forward declarations
class FontAtlas;
struct GlyphInfo;

// Rendering configuration
struct RenderConfig {
    float cell_width = 9.0f;
    float cell_height = 17.0f;
    float font_size = 14.0f;
    std::string font_path = "";
    bool use_bold_font = true;
    bool use_italic_font = true;
    bool use_ligatures = false;
    int oversample = 2;
    Color background_color = colors::BLACK;
};

// GPU cell data (for instanced rendering)
struct GPUCell {
    float x, y;           // Position
    float u, v;           // Texture coordinates
    float w, h;           // Glyph size
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t glyph_index;
    uint32_t flags;       // Bold, italic, etc.
};

// OpenGL renderer for terminal
class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    
    // Initialize renderer
    bool initialize(int viewport_width, int viewport_height);
    void shutdown();
    
    // Load font
    bool loadFont(const std::string& path, float size);
    bool loadSystemFont();
    
    // Configure
    void setConfig(const RenderConfig& config) { config_ = config; }
    const RenderConfig& getConfig() const { return config_; }
    
    // Render screen
    void render(const Screen& screen);
    
    // Viewport
    void setViewport(int width, int height);
    void getViewport(int& width, int& height) const { width = viewport_width_; height = viewport_height_; }
    
    // Calculate terminal size from viewport
    int calcCols() const { return static_cast<int>(viewport_width_ / config_.cell_width); }
    int calcRows() const { return static_cast<int>(viewport_height_ / config_.cell_height); }
    
    // Clear screen
    void clear();
    
    // Performance metrics
    struct Metrics {
        int draw_calls = 0;
        int glyphs_rendered = 0;
        float frame_time_ms = 0.0f;
    };
    const Metrics& getMetrics() const { return metrics_; }
    
private:
    RenderConfig config_;
    
    // Viewport
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    
    // Font atlas
    std::unique_ptr<FontAtlas> font_atlas_;
    
    // OpenGL objects
    GLuint shader_program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLuint glyph_texture_ = 0;
    GLuint ubo_ = 0;  // Uniform buffer for cell data
    
    // Shader uniforms
    GLint u_projection_ = -1;
    GLint u_cell_size_ = -1;
    GLint u_atlas_size_ = -1;
    
    // Cell buffer for instancing
    std::vector<GPUCell> cell_buffer_;
    size_t cell_buffer_capacity_ = 0;
    
    // Metrics
    Metrics metrics_;
    
    // Shader sources
    static const char* vertex_shader_source_;
    static const char* fragment_shader_source_;
    
    // Helpers
    bool createShaderProgram();
    bool createBuffers();
    void updateGlyphTexture();
    void uploadCells(const std::vector<GPUCell>& cells);
    GLuint compileShader(GLenum type, const char* source);
    
    // Build cell data from screen
    void buildCellData(const Screen& screen);
};

// Font atlas for glyph caching
class FontAtlas {
public:
    FontAtlas();
    ~FontAtlas();
    
    bool initialize(float size);
    void shutdown();
    
    // Get or render glyph
    const GlyphInfo* getGlyph(char32_t codepoint, bool bold, bool italic);
    
    // Texture info
    GLuint getTexture() const { return texture_; }
    int getTextureWidth() const { return texture_width_; }
    int getTextureHeight() const { return texture_height_; }
    
    // Metrics
    float getAscent() const { return ascent_; }
    float getDescent() const { return descent_; }
    float getLineGap() const { return line_gap_; }
    float getCellWidth() const { return cell_width_; }
    float getCellHeight() const { return cell_height_; }
    
private:
    struct AtlasNode {
        int x, y, width;
    };
    
    float font_size_ = 14.0f;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float line_gap_ = 0.0f;
    float cell_width_ = 9.0f;
    float cell_height_ = 17.0f;
    
    GLuint texture_ = 0;
    int texture_width_ = 1024;
    int texture_height_ = 1024;
    
    std::vector<AtlasNode> nodes_;
    std::unordered_map<uint64_t, GlyphInfo> glyphs_;
    
    // Platform-specific font handle
    #if defined(TX_ANDROID)
    void* font_face_ = nullptr;  // FT_Face
    #else
    void* ft_face_ = nullptr;
    void* hb_font_ = nullptr;
    #endif
    
    bool packGlyph(int width, int height, int& x, int& y);
    bool renderGlyph(char32_t codepoint, bool bold, bool italic, GlyphInfo& info);
};

// Glyph information
struct GlyphInfo {
    float u0, v0;       // Top-left texture coordinate
    float u1, v1;       // Bottom-right texture coordinate
    float x0, y0;       // Offset from baseline
    float x1, y1;       // Size
    float advance;      // Horizontal advance
};

} // namespace tx
