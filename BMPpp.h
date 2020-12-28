#ifndef _BMPpp_H_
#define _BMPpp_H_

#include "BMP.h"
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include "SdFat.h"

// BMP++
namespace BMPpp {
    class BMPpp {
        public:
            BMP getBMP() {
                return _bmp;
            }


        BMPpp() {

        }

        BMPpp(SdExFat &sd, const char *fname) {
            if (!read(sd, fname)) __builtin_abort();
        }

        bool read(SdExFat &sd, const char *fname) {
            ExFile inp = sd.open(fname, FILE_READ);
            if (!inp) {
                this->BMPErrorCode = "Unable to open the input image file.";
                return false;
            }
            uint64_t fileSize = inp.size();
            unsigned char bmpBytes[fileSize];
            unsigned int count = inp.read(bmpBytes, fileSize);
            uint64_t offset = 0;
            if (count != fileSize) {
                this->BMPErrorCode = "Read fewer bytes than expected";
                return false;
            }
            inp.close();

            std::memcpy((char *) &_bmp.file_header, bmpBytes, sizeof(_bmp.file_header));
            offset = sizeof(_bmp.file_header);

            if (_bmp.file_header.file_type != 0x4D42) {
                this->BMPErrorCode = "Error! Unrecognized file format.";
                return false;
            }
            std::memcpy((char *) &_bmp.bmp_info_header, bmpBytes, sizeof(_bmp.bmp_info_header));
            offset = sizeof(_bmp.bmp_info_header);

            // The BMPColorHeader is used only for transparent images
            if (_bmp.bmp_info_header.bit_count == 32) {
                // Check if the file has bit mask color information
                if (_bmp.bmp_info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader))) {
                    std::memcpy((char *) &_bmp.bmp_color_header, bmpBytes + offset, sizeof(_bmp.bmp_color_header));
                    offset += sizeof(_bmp.bmp_color_header);
                    // Check if the pixel data is stored as BGRA and if the color space type is sRGB
                    check_color_header(_bmp.bmp_color_header);
                } else {
                    this->BMPErrorCode = "Error! The file \"" + std::string(fname) +
                                "\" does not seem to contain bit mask information\n";
                    return false;
                }
            }

            // Jump to the pixel data location
            offset = _bmp.file_header.offset_data;

            // Adjust the header fields for output.
            // Some editors will put extra info in the image file, we only save the headers and the data.
            if (_bmp.bmp_info_header.bit_count == 32) {
                _bmp.bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
                _bmp.file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
            } else {
                _bmp.bmp_info_header.size = sizeof(BMPInfoHeader);
                _bmp.file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
            }
            _bmp.file_header.file_size = _bmp.file_header.offset_data;

            if (_bmp.bmp_info_header.height < 0) {
                this->BMPErrorCode = "The program can treat only BMP images with the origin in the bottom left corner!";
                return false;
            }

            _bmp.data.resize(_bmp.bmp_info_header.width * _bmp.bmp_info_header.height * _bmp.bmp_info_header.bit_count / 8);

            // Here we check if we need to take into account row padding
            if (_bmp.bmp_info_header.width % 4 == 0) {
                std::memcpy((char *) _bmp.data.data(), bmpBytes + offset, _bmp.data.size());
                offset += _bmp.data.size();
                _bmp.file_header.file_size += static_cast<uint32_t>(_bmp.data.size());
            } else {
                row_stride = _bmp.bmp_info_header.width * _bmp.bmp_info_header.bit_count / 8;
                uint32_t new_stride = make_stride_aligned(4);
                std::vector <uint8_t> padding_row(new_stride - row_stride);

                for (int y = 0; y < _bmp.bmp_info_header.height; ++y) {
                    std::memcpy((char *) (_bmp.data.data() + row_stride * y), bmpBytes + offset, row_stride);
                    offset += row_stride;
                    std::memcpy((char *) padding_row.data(), bmpBytes + offset, padding_row.size());
                    offset += padding_row.size();
                }
                _bmp.file_header.file_size += static_cast<uint32_t>(_bmp.data.size()) +
                                        _bmp.bmp_info_header.height * static_cast<uint32_t>(padding_row.size());
            }
            return true;
        }

        BMPpp(int32_t width, int32_t height, bool has_alpha = true) {
            if (width <= 0 || height <= 0) {
                this->BMPErrorCode = "The image width and height must be positive numbers.";
                __builtin_abort();
            }

            _bmp.bmp_info_header.width = width;
            _bmp.bmp_info_header.height = height;
            if (has_alpha) {
                _bmp.bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
                _bmp.file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);

                _bmp.bmp_info_header.bit_count = 32;
                _bmp.bmp_info_header.compression = 3;
                row_stride = width * 4;
                _bmp.data.resize(row_stride * height);
                _bmp.file_header.file_size = _bmp.file_header.offset_data + _bmp.data.size();
            } else {
                _bmp.bmp_info_header.size = sizeof(BMPInfoHeader);
                _bmp.file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

                _bmp.bmp_info_header.bit_count = 24;
                _bmp.bmp_info_header.compression = 0;
                row_stride = width * 3;
                _bmp.data.resize(row_stride * height);

                uint32_t new_stride = make_stride_aligned(4);
                _bmp.file_header.file_size = _bmp.file_header.offset_data + static_cast<uint32_t>(_bmp.data.size()) +
                                        _bmp.bmp_info_header.height * (new_stride - row_stride);
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
            if (x0 + w > (uint32_t) _bmp.bmp_info_header.width || y0 + h > (uint32_t) _bmp.bmp_info_header.height) {
                this->BMPErrorCode = "The region does not fit in the image!";
                return false;
            }

            uint32_t channels = _bmp.bmp_info_header.bit_count / 8;
            for (uint32_t y = y0; y < y0 + h; ++y) {
                for (uint32_t x = x0; x < x0 + w; ++x) {
                    _bmp.data[channels * (y * _bmp.bmp_info_header.width + x) + 0] = B;
                    _bmp.data[channels * (y * _bmp.bmp_info_header.width + x) + 1] = G;
                    _bmp.data[channels * (y * _bmp.bmp_info_header.width + x) + 2] = R;
                    if (channels == 4) {
                        _bmp.data[channels * (y * _bmp.bmp_info_header.width + x) + 3] = A;
                    }
                }
            }
            return true;
        }

        bool set_pixel(uint32_t x0, uint32_t y0, uint8_t B, uint8_t G, uint8_t R, uint8_t A) {
            if (x0 >= (uint32_t) _bmp.bmp_info_header.width || y0 >= (uint32_t) _bmp.bmp_info_header.height || x0 < 0 || y0 < 0) {
                this->BMPErrorCode = "The point is outside the image boundaries!";
                return false;
            }

            uint32_t channels = _bmp.bmp_info_header.bit_count / 8;
            _bmp.data[channels * (y0 * _bmp.bmp_info_header.width + x0) + 0] = B;
            _bmp.data[channels * (y0 * _bmp.bmp_info_header.width + x0) + 1] = G;
            _bmp.data[channels * (y0 * _bmp.bmp_info_header.width + x0) + 2] = R;
            if (channels == 4) {
                _bmp.data[channels * (y0 * _bmp.bmp_info_header.width + x0) + 3] = A;
            }
            return true;
        }

        bool draw_rectangle(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                            uint8_t B, uint8_t G, uint8_t R, uint8_t A, uint8_t line_w) {
            if (x0 + w > (uint32_t) _bmp.bmp_info_header.width || y0 + h > (uint32_t) _bmp.bmp_info_header.height) {
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
            of.write((const char *) &_bmp.file_header, sizeof(_bmp.file_header));
            of.write((const char *) &_bmp.bmp_info_header, sizeof(_bmp.bmp_info_header));
            if (_bmp.bmp_info_header.bit_count == 32) {
                of.write((const char *) &_bmp.bmp_color_header, sizeof(_bmp.bmp_color_header));
            }
        }

        void write_headers_and_data(std::ofstream &of) {
            write_headers(of);
            of.write((const char *) _bmp.data.data(), _bmp.data.size());
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

        BMP _bmp;
        std::string BMPErrorCode;
    };
}

#endif // _BMPpp_H_