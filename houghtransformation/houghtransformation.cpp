// houghtransformation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <unordered_map>
#include <cmath>

#include <Windows.h>

using namespace cv;
using namespace std;

const char* main_window = "Display window";
const char* edge_window = "Edge window";
const char* inv_window = "Inv window";
const char* line_window = "Line window";
const char* line2_window = "Line2 window";

#define HOUGH_WIDTH 800
#define HOUGH_HEIGHT 800

#define PRECISION_R 1
#define PRECISION_ANGLE 100

#define ANGLE_RESOLUTION 180 // else we have vertical lines in the hough image 


float calc_r(float cos_angle, float sin_angle, int x, int y) {
	return x * cos_angle + y * sin_angle;
}

int hough_coord_x(float angle, int width) {
	return angle / CV_PI * width;
}

int hough_coord_y(float r, int height, float max_r) {
	return height / 2 * (1 + r / max_r);
}

#define HOUGH_SPACE_VALUES_ARRAY_INDEX(angle, r, max_angle, max_r, hough_width, hough_height) round(((angle / max_angle) * hough_width - 1) + ((r + max_r) / (max_r * 2) * hough_height - 1) * hough_width)

int hough_space_values_array_index(float angle, float r, float max_angle, float max_r, int hough_width, int hough_height) {
	float normed_r = (r + max_r) / 2;
	int i_x = round((angle / max_angle)*(hough_width - 1));
	int i_y = round((normed_r / max_r)*(hough_height - 1));
	return i_x + i_y * hough_width;
}

struct hough_pair {
	int voting;
	int index;
};

int voting_desc_comparator(const void* p1, const void* p2) {
	return ((hough_pair*)p2)->voting - ((hough_pair*)p1)->voting;
}

#define BETWEEN(a, mi, ma) mi < a && a < ma


int main(int argc, char** argv)
{
	if (argc != 2)
	{
		cout << " Usage: display_image ImageToLoadAndDisplay" << endl;
		return -1;
	}

	Mat image;
	image = imread(argv[1], IMREAD_COLOR); // Read the file

	uchar* pixel = image.data + 200 * 1440 + 300 * 3; // this is the pixel at x: 300, y: 200 (first pixel is 0,0)

	if (!image.data) // Check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		return -1;
	}

	namedWindow(main_window, WINDOW_AUTOSIZE); // Create a window for display.
	imshow(main_window, image); // Show our image inside it.

								// gray scaling
	Mat gray;
	cvtColor(image, gray, COLOR_BGR2GRAY);

	// bluring
	Mat blured;
	Size sz = Size(11, 11);
	GaussianBlur(gray, blured, sz, 0, 0);

	// Edge detection
	Mat edges;
	double high_threshold = 255;
	double low_threshold = high_threshold / 3;
	Canny(blured, edges, low_threshold, high_threshold);
	//edges = blured;

	namedWindow(edge_window, WINDOW_AUTOSIZE); // Create a window for display.
	imshow(edge_window, edges); // Show our image inside it.


								// hough
	int rows = edges.rows;
	int cols = edges.cols;

	float angle_top_right_corner = atan((float)rows / cols);
	float max_r = cols * cos(angle_top_right_corner) + rows * sin(angle_top_right_corner);
	float max_angle = CV_PI;

	// hough image
	Mat hough_space = Mat(HOUGH_HEIGHT, HOUGH_WIDTH, CV_8UC1);
	hough_space = Mat::zeros(HOUGH_HEIGHT, HOUGH_WIDTH, CV_8UC1);

	const float ANGLE_INC = CV_PI / ANGLE_RESOLUTION;


	// get ticks per second & start timer
	LARGE_INTEGER frequency;
	LARGE_INTEGER t1, t2;
	double elapsedTime;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&t1);

	hough_pair* hough_space_values = (hough_pair*)calloc(HOUGH_HEIGHT * HOUGH_WIDTH, sizeof(hough_pair));

	int step_y = edges.step[0];
	int step_x = edges.step[1];

	// chache sin and cos value
	float sin_cache[ANGLE_RESOLUTION];
	float cos_cache[ANGLE_RESOLUTION];
	int angle_index = 0;
	for (int i = 0; i < ANGLE_RESOLUTION; i++) {
		float cur_angle = ANGLE_INC * i;
		sin_cache[angle_index] = sin(cur_angle);
		cos_cache[angle_index] = cos(cur_angle);
		angle_index++;
	}

	// loop over all pixels
	uchar* edge_pixel_value = edges.data;
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			// edge_pixel_value is only possible if we loop first over y and the over x!
			if (*edge_pixel_value++ > 0) {
				for (int i = 0; i < ANGLE_RESOLUTION; i++) {

					float cur_r = calc_r(cos_cache[i], sin_cache[i], x, y);
					float cur_angle = ANGLE_INC * i;
					// hough values
					int hough_values_address = hough_space_values_array_index(cur_angle, cur_r, max_angle, max_r, HOUGH_WIDTH, HOUGH_HEIGHT);
					(hough_space_values + hough_values_address)->voting += 1;
					(hough_space_values + hough_values_address)->index = hough_values_address;

					// drawing
					//int hough_x = hough_coord_x(cur_angle, HOUGH_WIDTH);
					//int hough_y = hough_coord_y(cur_r, HOUGH_HEIGHT, max_r);

					//uchar* hough_pixel_value = hough_space.data + hough_space.step[0] * hough_y + hough_space.step[1] + hough_x;
					//if (*(hough_pixel_value) < 0xFF) {
					//	*(hough_pixel_value) += 0x01;
					//}
				}
			}
		}
	}

	// sort by voting
	qsort(hough_space_values, HOUGH_HEIGHT * HOUGH_WIDTH, sizeof(hough_pair), voting_desc_comparator);

	// stop timer
	QueryPerformanceCounter(&t2);

	// compute and print the elapsed time in millisec
	elapsedTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;

	cout << "time: " << elapsedTime << endl;

	for (int i = 0; i < 5; i++) {
		int index = (hough_space_values + i)->index;

		int w_index = index % HOUGH_WIDTH;
		int h_index = (index - w_index) / HOUGH_WIDTH;

		float a = w_index * max_angle / (HOUGH_WIDTH - 1);
		float r = h_index * max_r / (HOUGH_HEIGHT - 1) * 2 - max_r;

		int sample_x_1 = 0;
		int sample_y_1 = r / sin(a);

		int sample_x_2 = cols;
		int sample_y_2 = (r - sample_x_2 * cos(a)) / sin(a);

		if (i == 0) {
			line(image, { sample_x_1, sample_y_1 }, { sample_x_2, sample_y_2 }, Scalar(255, 0, 0), 1);
		}
		else if (i == 1) {
			line(image, { sample_x_1, sample_y_1 }, { sample_x_2, sample_y_2 }, Scalar(0, 255, 0), 1);
		}
		else if (i == 2) {
			line(image, { sample_x_1, sample_y_1 }, { sample_x_2, sample_y_2 }, Scalar(0, 0, 255), 1);
		}
		else {
			line(image, { sample_x_1, sample_y_1 }, { sample_x_2, sample_y_2 }, Scalar(255, 0, 255), 1);
		}

		cout << (hough_space_values + i)->voting << ": " << index << "=" << w_index << "," << h_index << "=> a: " << a << ", r: " << r << "=> P1(" << sample_x_1 << "," << sample_y_1 << ")" << "=> P2(" << sample_x_2 << "," << sample_y_2 << ")" << endl;
	}

	namedWindow(inv_window, WINDOW_AUTOSIZE);
	imshow(inv_window, hough_space);

	namedWindow(line_window, WINDOW_AUTOSIZE);
	imshow(line_window, image);

	// get ticks per second & start timer
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&t1);

	vector<Vec2f> lines;
	HoughLines(edges, lines, 1, CV_PI / 180, 200, 0, 0);

	for (size_t i = 0; i < lines.size(); i++)
	{
		float rho = lines[i][0], theta = lines[i][1];
		Point pt1, pt2;
		double a = cos(theta), b = sin(theta);
		double x0 = a*rho, y0 = b*rho;
		pt1.x = cvRound(x0 + 1000 * (-b));
		pt1.y = cvRound(y0 + 1000 * (a));
		pt2.x = cvRound(x0 - 1000 * (-b));
		pt2.y = cvRound(y0 - 1000 * (a));
		line(image, pt1, pt2, Scalar(0, 0, 255), 1, CV_AA);
	}

	// stop timer & compute and print the elapsed time in millisec
	QueryPerformanceCounter(&t2);
	elapsedTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
	cout << "opencv time: " << elapsedTime << endl;

	namedWindow(line2_window, WINDOW_AUTOSIZE);
	imshow(line2_window, image);

	waitKey(0); // Wait for a keystroke in the window
	return 0;
}