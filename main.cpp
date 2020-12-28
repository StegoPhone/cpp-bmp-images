#include <iostream>
#include "BMP.h"

int main() {
    SdExFat sd = SdExFat();

	BMP bmp9(sd, "t1_24.bmp");
	bmp9.fill_region(0, 0, 50, 50, 0, 0, 255, 255);
	bmp9.fill_region(150, 0, 100, 150, 0, 255, 0, 255);
	bmp9.write(sd,"t1_24_copy.bmp");

	BMP bmp10(sd, "t2_24.bmp");
	bmp10.fill_region(0, 0, 50, 50, 0, 0, 255, 255);
	bmp10.fill_region(150, 0, 100, 150, 0, 255, 0, 255);
	bmp10.write(sd,"t2_24_copy.bmp");

	BMP bmp5(sd, "Shapes_24.bmp");
	bmp5.fill_region(0, 0, 100, 200, 0, 0, 255, 255);
	bmp5.fill_region(150, 0, 209, 203, 0, 255, 0, 255);
	bmp5.write(sd,"Shapes_24_copy.bmp");

	// Read an image from disk and write it back:
	BMP bmp(sd, "Shapes.bmp");
	bmp.fill_region(0, 0, 100, 200, 0, 0, 255, 255);
	bmp.write(sd, "Shapes_copy.bmp");

	// Create a BMP image in memory, modify it, save it on disk
	BMP bmp2(800, 600);
	bmp2.fill_region(50, 20, 100, 200, 0, 0, 255, 255);
	bmp2.write(sd,"img_test.bmp");

	// Create a 24 bits/pixel BMP image in memory, modify it, save it on disk
	BMP bmp3(200, 200, false);
	bmp3.fill_region(50, 20, 100, 100, 255, 0, 255, 255);
	bmp3.write(sd,"img_test_24bits.bmp");

	BMP bmp4(sd,"img_test_24bits.bmp");
	bmp4.write(sd,"img_test_24bits_2.bmp");

	BMP bmp6(403, 305, false);
	bmp6.fill_region(0, 0, 50, 50, 0, 0, 255, 0);
	bmp6.write(sd,"test6.bmp");

	BMP bmp7(sd,"test6.bmp");
	bmp7.fill_region(0, 0, 40, 40, 255, 0, 0, 0);
	bmp7.write(sd,"test6_2.bmp");

	BMP bmp8(200, 200, false);
	bmp8.fill_region(0, 0, 100, 100, 255, 0, 255, 255);
	bmp8.write(sd,"img_test_24bits_3.bmp");

	BMP bmp11(sd,"test_pnet.bmp");
	bmp11.fill_region(0, 0, 100, 100, 255, 0, 255, 255);
	bmp11.write(sd,"test_pnet_copy.bmp");
}
