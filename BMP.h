#ifndef _BMP_H_
#define _BMP_H_

#pragma once

#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include "SdFat.h"

static std::string BMPErrorCode;

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type{
            0x4D42};          // File type always BM which is 0x4D42 (stored as hex uint16_t in little endian)
    uint32_t file_size{0};               // Size of the file (in bytes)
    uint16_t reserved1{0};               // Reserved, always 0
    uint16_t reserved2{0};               // Reserved, always 0
    uint32_t offset_data{0};             // Start position of pixel data (bytes from the beginning of the file)
};

struct BMPInfoHeader {
    uint32_t size{0};                      // Size of this header (in bytes)
    int32_t width{0};                      // width of bitmap in pixels
    int32_t height{0};                     // width of bitmap in pixels
    //       (if positive, bottom-up, with origin in lower left corner)
    //       (if negative, top-down, with origin in upper left corner)
    uint16_t planes{1};                    // No. of planes for the target device, this is always 1
    uint16_t bit_count{0};                 // No. of bits per pixel
    uint32_t compression{0};               // 0 or 3 - uncompressed. THIS PROGRAM CONSIDERS ONLY UNCOMPRESSED BMP images
    uint32_t size_image{0};                // 0 - for uncompressed images
    int32_t x_pixels_per_meter{0};
    int32_t y_pixels_per_meter{0};
    uint32_t colors_used{
            0};               // No. color indexes in the color table. Use 0 for the max number of colors allowed by bit_count
    uint32_t colors_important{0};          // No. of colors used for displaying the bitmap. If 0 all colors are required
};

struct BMPColorHeader {
    uint32_t red_mask{0x00ff0000};         // Bit mask for the red channel
    uint32_t green_mask{0x0000ff00};       // Bit mask for the green channel
    uint32_t blue_mask{0x000000ff};        // Bit mask for the blue channel
    uint32_t alpha_mask{0xff000000};       // Bit mask for the alpha channel
    uint32_t color_space_type{0x73524742}; // Default "sRGB" (0x73524742)
    uint32_t unused[16]{0};                // Unused data for sRGB color space
};
#pragma pack(pop)

struct BMP {
    BMPFileHeader file_header;
    BMPInfoHeader bmp_info_header;
    BMPColorHeader bmp_color_header;
    std::vector <uint8_t> data;

    BMP() {

    }

    BMP(SdExFat &sd, const char *fname) {
        if (!read(sd, fname)) __builtin_abort();
    }

    bool read(SdExFat &sd, const char *fname) {
        ExFile inp = sd.open(fname, FILE_READ);
        if (!inp) {
            BMPErrorCode = "Unable to open the input image file.";
            return false;
        }
        uint64_t fileSize = inp.size();
        unsigned char bmpBytes[fileSize];
        unsigned int count = inp.read(bmpBytes, fileSize);
        uint64_t offset = 0;
        if (count != fileSize) {
            BMPErrorCode = "Read fewer bytes than expected";
            return false;
        }
        inp.close();

        memcpy((char *) &file_header, bmpBytes, sizeof(file_header));
        offset = sizeof(file_header);

        if (file_header.file_type != 0x4D42) {
            BMPErrorCode = "Error! Unrecognized file format.";
            return false;
        }
        memcpy((char *) &bmp_info_header, bmpBytes, sizeof(bmp_info_header));
        offset = sizeof(bmp_info_header);

        // The BMPColorHeader is used only for transparent images
        if (bmp_info_header.bit_count == 32) {
            // Check if the file has bit mask color information
            if (bmp_info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader))) {
                memcpy((char *) &bmp_color_header, bmpBytes + offset, sizeof(bmp_color_header));
                offset += sizeof(bmp_color_header);
                // Check if the pixel data is stored as BGRA and if the color space type is sRGB
                check_color_header(bmp_color_header);
            } else {
                BMPErrorCode = "Error! The file \"" + std::string(fname) +
                               "\" does not seem to contain bit mask information\n";
                return false;
            }
        }

        // Jump to the pixel data location
        offset = file_header.offset_data;

        // Adjust the header fields for output.
        // Some editors will put extra info in the image file, we only save the headers and the data.
        if (bmp_info_header.bit_count == 32) {
            bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
            file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
        } else {
            bmp_info_header.size = sizeof(BMPInfoHeader);
            file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
        }
        file_header.file_size = file_header.offset_data;

        if (bmp_info_header.height < 0) {
            BMPErrorCode = "The program can treat only BMP images with the origin in the bottom left corner!";
            return false;
        }

        data.resize(bmp_info_header.width * bmp_info_header.height * bmp_info_header.bit_count / 8);

        // Here we check if we need to take into account row padding
        if (bmp_info_header.width % 4 == 0) {
            memcpy((char *) data.data(), bmpBytes + offset, data.size());
            offset += data.size();
            file_header.file_size += static_cast<uint32_t>(data.size());
        } else {
            row_stride = bmp_info_header.width * bmp_info_header.bit_count / 8;
            uint32_t new_stride = make_stride_aligned(4);
            std::vector <uint8_t> padding_row(new_stride - row_stride);

            for (int y = 0; y < bmp_info_header.height; ++y) {
                memcpy((char *) (data.data() + row_stride * y), bmpBytes + offset, row_stride);
                offset += row_stride;
                memcpy((char *) padding_row.data(), bmpBytes + offset, padding_row.size());
                offset += padding_row.size();
            }
            file_header.file_size += static_cast<uint32_t>(data.size()) +
                                     bmp_info_header.height * static_cast<uint32_t>(padding_row.size());
        }
        return true;
    }

    BMP(int32_t width, int32_t height, bool has_alpha = true) {
        if (width <= 0 || height <= 0) {
            BMPErrorCode = "The image width and height must be positive numbers.";
            __builtin_abort();
        }

        bmp_info_header.width = width;
        bmp_info_header.height = height;
        if (has_alpha) {
            bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
            file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);

            bmp_info_header.bit_count = 32;
            bmp_info_header.compression = 3;
            row_stride = width * 4;
            data.resize(row_stride * height);
            file_header.file_size = file_header.offset_data + data.size();
        } else {
            bmp_info_header.size = sizeof(BMPInfoHeader);
            file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

            bmp_info_header.bit_count = 24;
            bmp_info_header.compression = 0;
            row_stride = width * 3;
            data.resize(row_stride * height);

            uint32_t new_stride = make_stride_aligned(4);
            file_header.file_size = file_header.offset_data + static_cast<uint32_t>(data.size()) +
                                    bmp_info_header.height * (new_stride - row_stride);
        }
    }

    bool write(SdExFat &sd, const char *fname) {
        return false;
//        std::ofstream of{ fname, std::ios_base::binary };
//        if (!of) {
//            BMPErrorCode = "Unable to open the output image file.";
//            return false;
//        }
//
//        if (bmp_info_header.bit_count == 32) {
//            write_headers_and_data(of);
//        }
//        else if (bmp_info_header.bit_count == 24) {
//            if (bmp_info_header.width % 4 == 0) {
//                write_headers_and_data(of);
//            }
//            else {
//                uint32_t new_stride = make_stride_aligned(4);
//                std::vector<uint8_t> padding_row(new_stride - row_stride);
//
//                write_headers(of);
//
//                for (int y = 0; y < bmp_info_header.height; ++y) {
//                    of.write((const char*)(data.data() + row_stride * y), row_stride);
//                    of.write((const char*)padding_row.data(), padding_row.size());
//                }
//            }
//        }
//        else {
//            BMPErrorCode = "The program can treat only 24 or 32 bits per pixel BMP files";
//            return false;
//        }
//
//        return true;
    }

    bool fill_region(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, uint8_t B, uint8_t G, uint8_t R, uint8_t A) {
        if (x0 + w > (uint32_t) bmp_info_header.width || y0 + h > (uint32_t) bmp_info_header.height) {
            BMPErrorCode = "The region does not fit in the image!";
            return false;
        }

        uint32_t channels = bmp_info_header.bit_count / 8;
        for (uint32_t y = y0; y < y0 + h; ++y) {
            for (uint32_t x = x0; x < x0 + w; ++x) {
                data[channels * (y * bmp_info_header.width + x) + 0] = B;
                data[channels * (y * bmp_info_header.width + x) + 1] = G;
                data[channels * (y * bmp_info_header.width + x) + 2] = R;
                if (channels == 4) {
                    data[channels * (y * bmp_info_header.width + x) + 3] = A;
                }
            }
        }
        return true;
    }

    bool set_pixel(uint32_t x0, uint32_t y0, uint8_t B, uint8_t G, uint8_t R, uint8_t A) {
        if (x0 >= (uint32_t) bmp_info_header.width || y0 >= (uint32_t) bmp_info_header.height || x0 < 0 || y0 < 0) {
            BMPErrorCode = "The point is outside the image boundaries!";
            return false;
        }

        uint32_t channels = bmp_info_header.bit_count / 8;
        data[channels * (y0 * bmp_info_header.width + x0) + 0] = B;
        data[channels * (y0 * bmp_info_header.width + x0) + 1] = G;
        data[channels * (y0 * bmp_info_header.width + x0) + 2] = R;
        if (channels == 4) {
            data[channels * (y0 * bmp_info_header.width + x0) + 3] = A;
        }
        return true;
    }

    bool draw_rectangle(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                        uint8_t B, uint8_t G, uint8_t R, uint8_t A, uint8_t line_w) {
        if (x0 + w > (uint32_t) bmp_info_header.width || y0 + h > (uint32_t) bmp_info_header.height) {
            BMPErrorCode = "The rectangle does not fit in the image!";
            return false;
        }

        bool result;
        result = fill_region(x0, y0, w, line_w, B, G, R, A);                                             // top line
        result |= fill_region(x0, (y0 + h - line_w), w, line_w, B, G, R, A);                              // bottom line
        result |= fill_region((x0 + w - line_w), (y0 + line_w), line_w, (h - (2 * line_w)), B, G, R, A);  // right line
        result |= fill_region(x0, (y0 + line_w), line_w, (h - (2 * line_w)), B, G, R, A);                 // left line
        return result;
    }

private:
    uint32_t row_stride{0};

    void write_headers(std::ofstream &of) {
        of.write((const char *) &file_header, sizeof(file_header));
        of.write((const char *) &bmp_info_header, sizeof(bmp_info_header));
        if (bmp_info_header.bit_count == 32) {
            of.write((const char *) &bmp_color_header, sizeof(bmp_color_header));
        }
    }

    void write_headers_and_data(std::ofstream &of) {
        write_headers(of);
        of.write((const char *) data.data(), data.size());
    }

    // Add 1 to the row_stride until it is divisible with align_stride
    uint32_t make_stride_aligned(uint32_t align_stride) {
        uint32_t new_stride = row_stride;
        while (new_stride % align_stride != 0) {
            new_stride++;
        }
        return new_stride;
    }

    // Check if the pixel data is stored as BGRA and if the color space type is sRGB
    bool check_color_header(BMPColorHeader &bmp_color_header) {
        BMPColorHeader expected_color_header;
        if (expected_color_header.red_mask != bmp_color_header.red_mask ||
            expected_color_header.blue_mask != bmp_color_header.blue_mask ||
            expected_color_header.green_mask != bmp_color_header.green_mask ||
            expected_color_header.alpha_mask != bmp_color_header.alpha_mask) {
            BMPErrorCode = "Unexpected color mask format! The program expects the pixel data to be in the BGRA format";
            return false;
        }
        if (expected_color_header.color_space_type != bmp_color_header.color_space_type) {
            BMPErrorCode = "Unexpected color space type! The program expects sRGB values";
            return false;
        }
        return true;
    }
};

#endif // _BMP_H_