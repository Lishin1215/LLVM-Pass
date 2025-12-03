/*
 * Filament Linear Resize
 * 
 * High quality image resize - using Filament's color conversion API
 */

#include <filament/Color.h>
#include <math/vec3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

using filament::math::float3;
using filament::Color;
using filament::LinearColor;
using filament::sRGBColor;
using filament::FAST;

static int is_png_file(const char* filename) {
    size_t len = strlen(filename);
    return len >= 4 && strcmp(filename + len - 4, ".png") == 0;
}

// Bilinear resize in linear space
static void bilinear_resize_linear(
    const LinearColor* src, int src_w, int src_h,
    LinearColor* dst, int dst_w, int dst_h)
{
    float x_ratio = (float)(src_w - 1) / dst_w;
    float y_ratio = (float)(src_h - 1) / dst_h;
    
    for (int y = 0; y < dst_h; y++) {
        float src_y = y * y_ratio;
        int y0 = (int)src_y;
        int y1 = y0 + 1;
        if (y1 >= src_h) y1 = src_h - 1;
        float fy = src_y - y0;
        
        for (int x = 0; x < dst_w; x++) {
            float src_x = x * x_ratio;
            int x0 = (int)src_x;
            int x1 = x0 + 1;
            if (x1 >= src_w) x1 = src_w - 1;
            float fx = src_x - x0;
            
            const LinearColor& p00 = src[y0 * src_w + x0];
            const LinearColor& p10 = src[y0 * src_w + x1];
            const LinearColor& p01 = src[y1 * src_w + x0];
            const LinearColor& p11 = src[y1 * src_w + x1];
            
            // Bilinear interpolation in linear space
            LinearColor result;
            result.x = p00.x * (1-fx) * (1-fy) + p10.x * fx * (1-fy) +
                       p01.x * (1-fx) * fy + p11.x * fx * fy;
            result.y = p00.y * (1-fx) * (1-fy) + p10.y * fx * (1-fy) +
                       p01.y * (1-fx) * fy + p11.y * fx * fy;
            result.z = p00.z * (1-fx) * (1-fy) + p10.z * fx * (1-fy) +
                       p01.z * (1-fx) * fy + p11.z * fx * fy;
            
            dst[y * dst_w + x] = result;
        }
    }
}

static int process_image(const char* input_path, const char* output_path,
                         int target_width, int target_height) {
    int width, height, channels;
    unsigned char* image = stbi_load(input_path, &width, &height, &channels, 3);
    if (!image) {
        printf("ERROR: Cannot load %s\n", input_path);
        return 1;
    }
    
    size_t src_pixels = (size_t)width * height;
    size_t dst_pixels = (size_t)target_width * target_height;
    
    sRGBColor* srgb_input = (sRGBColor*)malloc(src_pixels * sizeof(sRGBColor));
    for (size_t i = 0; i < src_pixels; i++) {
        srgb_input[i].x = image[i * 3 + 0] / 255.0f;
        srgb_input[i].y = image[i * 3 + 1] / 255.0f;
        srgb_input[i].z = image[i * 3 + 2] / 255.0f;
    }
    stbi_image_free(image);
    
    LinearColor* linear_src = (LinearColor*)malloc(src_pixels * sizeof(LinearColor));
    LinearColor* linear_dst = (LinearColor*)malloc(dst_pixels * sizeof(LinearColor));
    sRGBColor* srgb_output = (sRGBColor*)malloc(dst_pixels * sizeof(sRGBColor));
    
    for (size_t i = 0; i < src_pixels; i++) {
        linear_src[i] = Color::toLinear<FAST>(srgb_input[i]);
    }
    
    bilinear_resize_linear(linear_src, width, height, linear_dst, target_width, target_height);
    
    for (size_t i = 0; i < dst_pixels; i++) {
        LinearColor clamped = linear_dst[i];
        if (clamped.x < 0) clamped.x = 0; if (clamped.x > 1) clamped.x = 1;
        if (clamped.y < 0) clamped.y = 0; if (clamped.y > 1) clamped.y = 1;
        if (clamped.z < 0) clamped.z = 0; if (clamped.z > 1) clamped.z = 1;
        srgb_output[i] = Color::toSRGB<FAST>(clamped);
    }
    
    unsigned char* output_bytes = (unsigned char*)malloc(dst_pixels * 3);
    for (size_t i = 0; i < dst_pixels; i++) {
        float r = srgb_output[i].x < 0 ? 0 : (srgb_output[i].x > 1 ? 1 : srgb_output[i].x);
        float g = srgb_output[i].y < 0 ? 0 : (srgb_output[i].y > 1 ? 1 : srgb_output[i].y);
        float b = srgb_output[i].z < 0 ? 0 : (srgb_output[i].z > 1 ? 1 : srgb_output[i].z);
        output_bytes[i * 3 + 0] = (unsigned char)(r * 255.0f + 0.5f);
        output_bytes[i * 3 + 1] = (unsigned char)(g * 255.0f + 0.5f);
        output_bytes[i * 3 + 2] = (unsigned char)(b * 255.0f + 0.5f);
    }
    stbi_write_png(output_path, target_width, target_height, 3, output_bytes, target_width * 3);
    printf("Resized %s (%dx%d) -> %s (%dx%d)\n", input_path, width, height, output_path, target_width, target_height);
    
    free(output_bytes);
    free(srgb_input);
    free(linear_src);
    free(linear_dst);
    free(srgb_output);
    return 0;
}

int main(int argc, char* argv[]) {
    const char* input_path = "../bench_4k.png";
    const char* output_path = "filament_resized.png";
    const char* input_dir = NULL;
    const char* output_dir = "/tmp";
    int target_width = 1920;
    int target_height = 1080;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) input_path = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output_path = argv[++i];
        else if (strcmp(argv[i], "--input-dir") == 0 && i + 1 < argc) input_dir = argv[++i];
        else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) output_dir = argv[++i];
        else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) target_width = atoi(argv[++i]);
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) target_height = atoi(argv[++i]);
    }
    
    if (input_dir) {
        DIR* dir = opendir(input_dir);
        if (!dir) { printf("ERROR: Cannot open %s\n", input_dir); return 1; }
        
        struct dirent* entry;
        int count = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (is_png_file(entry->d_name)) {
                char in[512], out[512];
                snprintf(in, sizeof(in), "%s/%s", input_dir, entry->d_name);
                snprintf(out, sizeof(out), "%s/resized_%s", output_dir, entry->d_name);
                process_image(in, out, target_width, target_height);
                count++;
            }
        }
        closedir(dir);
        printf("Processed %d images\n", count);
        return 0;
    }
    
    return process_image(input_path, output_path, target_width, target_height);
}
