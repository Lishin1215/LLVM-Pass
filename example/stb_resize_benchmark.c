/*
 * STB Linear Resize
 * 
 * High quality image resize - performs resize in linear space
 * Supports processing entire directories of images
 */

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Linear -> sRGB using same logic as STB's stbi__hdr_to_ldr
// Reference: stb_image.h line 1883: pow(data[i]*scale, gamma) * 255
static void linear_to_srgb_buffer(const float* src, unsigned char* dst, 
                                   size_t count, float gamma) {
    for (size_t i = 0; i < count; i++) {
        float z = (float)pow(src[i], gamma) * 255.0f + 0.5f;
        if (z < 0) z = 0;
        if (z > 255) z = 255;
        dst[i] = (unsigned char)z;
    }
}

// Simple bilinear resize (in linear space)
static void bilinear_resize_linear(
    const float* src, int src_w, int src_h,
    float* dst, int dst_w, int dst_h, int channels)
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
            
            for (int c = 0; c < channels; c++) {
                float p00 = src[(y0 * src_w + x0) * channels + c];
                float p10 = src[(y0 * src_w + x1) * channels + c];
                float p01 = src[(y1 * src_w + x0) * channels + c];
                float p11 = src[(y1 * src_w + x1) * channels + c];
                
                // Bilinear interpolation
                float val = p00 * (1-fx) * (1-fy) + 
                           p10 * fx * (1-fy) +
                           p01 * (1-fx) * fy +
                           p11 * fx * fy;
                
                dst[(y * dst_w + x) * channels + c] = val;
            }
        }
    }
}

// Process single image
static int process_image(const char* input_path, const char* output_path, 
                         int target_width, int target_height) {
    // Set STB's LDR->HDR gamma (default is 2.2)
    stbi_ldr_to_hdr_gamma(2.2f);
    stbi_ldr_to_hdr_scale(1.0f);
    
    int width, height, channels;
    
    // Step 1: Load and convert to linear space (using STB's stbi_loadf)
    float* linear_src = stbi_loadf(input_path, &width, &height, &channels, 3);
    
    if (!linear_src) {
        printf("ERROR: Cannot load %s\n", input_path);
        return 1;
    }
    channels = 3;
    
    size_t dst_pixels = (size_t)target_width * target_height;
    
    float* linear_dst = (float*)malloc(dst_pixels * channels * sizeof(float));
    unsigned char* output = (unsigned char*)malloc(dst_pixels * channels);
    
    // Step 2: Bilinear resize in linear space
    bilinear_resize_linear(linear_src, width, height,
                           linear_dst, target_width, target_height, channels);
    
    // Step 3: Linear -> sRGB (pow 1/2.2)
    linear_to_srgb_buffer(linear_dst, output, dst_pixels * channels, 1.0f / 2.2f);
    
    // Step 4: Save result
    stbi_write_png(output_path, target_width, target_height, channels, output, target_width * channels);
    printf("Resized %s (%dx%d) -> %s (%dx%d)\n", input_path, width, height, output_path, target_width, target_height);
    
    stbi_image_free(linear_src);
    free(linear_dst);
    free(output);
    
    return 0;
}

// Check if file is PNG
static int is_png_file(const char* filename) {
    size_t len = strlen(filename);
    if (len < 4) return 0;
    return strcmp(filename + len - 4, ".png") == 0;
}

int main(int argc, char* argv[]) {
    const char* input_path = "../bench_4k.png";
    const char* output_path = "stb_resized.png";
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
    
    // If directory specified, process entire directory
    if (input_dir != NULL) {
        DIR* dir = opendir(input_dir);
        if (!dir) {
            printf("ERROR: Cannot open directory %s\n", input_dir);
            return 1;
        }
        
        struct dirent* entry;
        int count = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            if (is_png_file(entry->d_name)) {
                char input_full[512], output_full[512];
                snprintf(input_full, sizeof(input_full), "%s/%s", input_dir, entry->d_name);
                snprintf(output_full, sizeof(output_full), "%s/resized_%s", output_dir, entry->d_name);
                
                process_image(input_full, output_full, target_width, target_height);
                count++;
            }
        }
        
        closedir(dir);
        printf("\nProcessed %d images from %s\n", count, input_dir);
        return 0;
    }
    
    // Single image mode
    return process_image(input_path, output_path, target_width, target_height);
}
