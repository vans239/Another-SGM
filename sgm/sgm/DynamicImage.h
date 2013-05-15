#pragma once
#include <queue>
#include <fstream>

#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/core/core.hpp>        
#include <opencv2/opencv.hpp>

#include "Timer.h"
#include "Properties.h"
#include "Utils.h"

;
using namespace cv;
using namespace std;

std::ofstream log_disp("img_disp.txt");


class DynamicDirection{
public:
	DynamicDirection(const Mat& left, const Mat& right, const CostCalculator& costs,  const Size& size, const int& max_disp, const int& penalty1, const int& penalty2) 
		: costs(costs), left(left), right(right), penalty1(penalty1), penalty2(penalty2), size(size), max_disp(max_disp) {}

	virtual MatND calculateL(const Point2i& direction){
		timer.start("Dynamic direction calculating");
		this->direction = direction;
		int sizes[3];
		sizes[0] = size.height;
		sizes[1] = size.width;
		sizes[2] = max_disp;

		l = MatND(3, sizes, CV_8U, Scalar(255));

		std::queue<Point2i> queue;
		for(int y = 0; y < size.height; ++y){
			for(int x = 0; x < size.width; ++x){
				const Point2i p(x,y);
				if(isStart(p)){
					queue.push(p);
				}
			}
		}
		timer.finish();

		timer.start("Phase2");

		while(!queue.empty()){
			const Point2i p = queue.front();
			queue.pop();
			handle(p);
			const Point2i next = nextPoint(p);
			if(isInside(next)){
				queue.push(next);
			}
		}
		timer.finish();
		return l;
	}

protected:
	void handle(const Point2i& p){
		const Point2i prev = prevPoint(p);

		const uchar* l_iter_prev;
		//todo prev not in mat?
		if(isInside(prev)) 
			l_iter_prev = l.ptr<uchar>(prev.y,prev.x);
		
		uchar* l_iter = l.ptr<uchar>(p.y,p.x);

		int fromDisp = getFromDisp(p);
		int toDisp = getToDisp(p);

		const uchar* curr_iter = costs.getCosts(p.y, p.x, fromDisp, toDisp);

		int min = 255;
		for(int i = fromDisp; i < toDisp; ++i){
			min = std::min(min, get(l_iter_prev, prev, i));
		}

		for(int d = fromDisp; d < toDisp; ++d){ //todo maybe here should be prev fromDisp and toDisp
			int curr = curr_iter[d]; 
			int a = get(l_iter_prev, prev,d);
			int b = get(l_iter_prev, prev,d - 1);
			int c = get(l_iter_prev, prev,d + 1);
			l_iter[d] = getBestValue(a,b,c,curr, min); 
		}
	}

	ushort getBestValue(int a, int b, int c, int curr, int min){
		int bestValue = a;
		bestValue = std::min(bestValue, min + penalty2);
		bestValue = std::min(bestValue, b + penalty1);
		bestValue = std::min(bestValue, c + penalty1);
		bestValue = curr + bestValue - min;
		bestValue = std::min(bestValue, 255);
		return bestValue;
	}

	virtual int getFromDisp(const Point2i& p){
		return 0;
	}
	virtual int getToDisp(const Point2i& p){
		return max_disp;
	}

	Point2i prevPoint(const Point2i& p){
		return p - direction;
	}
	Point2i nextPoint(const Point2i& p){
		return p + direction;
	}
	bool isInside(const Point2i& p){
		if(p.x < 0 || p.y < 0 || p.x >= size.width || p.y >= size.height ){
			return false;
		}
		return true;
	}

	bool isStart(const Point2i& p){
		return !isInside(prevPoint(p));
	}

	int get(const uchar* l_iter, const Point2i& p, const int& d){
		if(!isInside(p)){
			return 255;
		}

		if(d < 0 ||  d >= max_disp){
			return 255;
		}
		return l_iter[d];
	}

	MatND l;
	const Mat left;
	const Mat right;
	const CostCalculator& costs;
	const Size size;
	const int max_disp;
	const int penalty1;
	const int penalty2;
	Point2i direction;
};


class DynamicImage {
public:
	DynamicImage(const int max_disp) : max_disp(max_disp){

	}
	//calculate disparity map via sgm
	// a,b - costs
	Mat calculateDisparity(const Size& size, DynamicDirection& dd){
		Mat s(size, CV_16U);
		int sizes[3];
		sizes[0] = size.height;
		sizes[1] = size.width;
		sizes[2] = max_disp;
		MatND sum(3, sizes, CV_16U, Scalar(0));
		for(int i = 0; i < DIRS; ++i){
			MatND l = dd.calculateL(Point2i(r_x[i], r_y[i]));
			timer.start("Sum calculating");
			//it's works O(W * H), but it depends on pixelwith disparity range less than 2 * delta
			for(int y = 0; y < size.height; ++y){
				for(int x = 0; x < size.width; ++x){
					ushort* sum_iter = sum.ptr<ushort>(y,x);
					const uchar* l_iter = l.ptr<uchar>(y,x);

					int fromDisp = getFromDisp(y,x);
					int toDisp = getToDisp(y,x);

					for(int d = fromDisp; d < toDisp; ++d){
						sum_iter[d] += l_iter[d];
					}
				}
			}
			timer.finish();
		}

		timer.start("Best calculating");
		for(int y = 0; y < size.height; ++y){
			for(int x = 0; x < size.width; ++x){
				int minD = -1;
				int min = 10000;
				int fromDisp = getFromDisp(y,x);
				int toDisp = getToDisp(y,x);
				ushort* sum_iter = sum.ptr<ushort>(y,x);
				for(int d = fromDisp; d < toDisp; ++d){
					if(min > sum_iter[d]){
						min = sum_iter[d];
						minD = d;
					}
				}
				*s.ptr<ushort>(y, x) = minD;
			}
		}
		timer.finish();
		return s;
	}
protected:
	const int max_disp;

	virtual int getFromDisp(const int& y, const int& x) const {
		return 0;
	}
	virtual int getToDisp(const int& y, const int& x) const {
		return max_disp;
	}
};