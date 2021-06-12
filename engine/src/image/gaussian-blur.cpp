// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./gaussian-blur.h"
#include <array>


static std::array<int, 3> BoxesForGauss(float sigma, int n) {
	float wIdeal = glm::sqrt((12 * sigma * sigma / n) + 1);
    int wl = (int)glm::floor(wIdeal);
	if ((wl & 1) == 0)
		--wl;
    int wu = wl + 2;

	float mIdeal = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
    int m = (int)glm::round(mIdeal);

	std::array<int, 3> result;
    for (int i = 0; i < n; ++i)
		result[(unsigned int)i] = (i < m ? wl : wu);
	return result;
}


static void BoxBlurH(glm::vec4* scl, glm::vec4* tcl, int w, int h, float r) {
	int rr = (int)glm::round(r);
	float iarr = 1.0f / (r + r + 1.0f);
	for(int i = 0; i < h; i++)
	{
		int ti = i * w, li = ti, ri = ti + rr;
		glm::vec4 fv = scl[ti], lv = scl[ti + w - 1], val = (r + 1.0f) * fv;
		for(int j = 0;                   j <  rr;     j++)   val += scl[ti + j];
		for(int j = 0;                   j <= rr;     j++) { val += scl[ri++] - fv;        tcl[ti++] = val * iarr; }
		for(int j = rr + 1; j <  w - rr; j++)              { val += scl[ri++] - scl[li++]; tcl[ti++] = val * iarr; }
		for(int j = w - rr; j <  w;      j++)              { val += lv        - scl[li++]; tcl[ti++] = val * iarr; }
	}

}


static void BoxBlurT(glm::vec4* scl, glm::vec4* tcl, int w, int h, float r) {
	int rr = (int)glm::round(r);
	float iarr = 1.0f / (r + r + 1.0f);
	for(int i = 0; i < w; i++)
	{
		int ti = i, li = ti, ri = ti + rr * w;
		glm::vec4 fv = scl[ti], lv = scl[ti+w*(h-1)], val = (r + 1.0f) * fv;
		for(int j = 0;                   j <  rr;     j++)   val += scl[ti + j * w];
		for(int j = 0;                   j <= rr;     j++) { val += scl[ri] - fv     ; tcl[ti] = val * iarr;  ri += w; ti += w; }
		for(int j = rr + 1; j <  h - rr; j++)              { val += scl[ri] - scl[li]; tcl[ti] = val * iarr;  li += w; ri += w; ti += w; }
		for(int j = h - rr; j <  h;      j++)              { val += lv      - scl[li]; tcl[ti] = val * iarr;  li += w; ti += w; }
	}
}


static void BoxBlur(glm::vec4* scl, glm::vec4* tcl, int w, int h, float r) {
	int s = w * h;
	for (int i = 0; i < s; ++i) tcl[i] = scl[i];
	BoxBlurH(tcl, scl, w, h, r);
	BoxBlurT(scl, tcl, w, h, r);
}


void GaussianBlur::apply(glm::vec4* scl, glm::vec4* tcl, int w, int h, float r) {
	std::array<int, 3> bxs = BoxesForGauss(r, 3);
	BoxBlur(scl, tcl, w, h, (bxs[0] - 1) / 2);
	BoxBlur(tcl, scl, w, h, (bxs[1] - 1) / 2);
	BoxBlur(scl, tcl, w, h, (bxs[2] - 1) / 2);
}
