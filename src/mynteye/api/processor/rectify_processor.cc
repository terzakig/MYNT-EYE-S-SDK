// Copyright 2018 Slightech Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "mynteye/api/processor/rectify_processor.h"

#include <utility>

#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "mynteye/logger.h"

MYNTEYE_BEGIN_NAMESPACE

cv::Mat RectifyProcessor::rectifyrad(const cv::Mat& R) {
  cv::Mat r_vec;
  cv::Rodrigues(R, r_vec);
  //  pi/180 = x/179 ==> x = 3.1241
  double rad = cv::norm(r_vec);
  if (rad >= 3.1241) {
    cv::Mat r_dir;
    cv::normalize(r_vec, r_dir);
    cv::Mat r = r_dir*(3.1415926 - rad);
    cv::Mat r_r;
    cv::Rodrigues(r, r_r);
    return r_r.clone();
  }
  return R.clone();
}

/*void RectifyProcessor::stereoRectify(models::CameraPtr leftOdo,
    models::CameraPtr rightOdo, const CvMat* K1, const CvMat* K2,
    const CvMat* D1, const CvMat* D2, CvSize imageSize,
    const CvMat* matR, const CvMat* matT,
    CvMat* _R1, CvMat* _R2, CvMat* _P1, CvMat* _P2, double* T_mul_f,
    double *cx1_min_cx2,
    int flags, double alpha, CvSize newImgSize) {
  // std::cout << _alpha << std::endl;
  alpha = _alpha;
  double _om[3], _t[3] = {0}, _uu[3]={0, 0, 0}, _r_r[3][3], _pp[3][4];
  double _ww[3], _wr[3][3], _z[3] = {0, 0, 0}, _ri[3][3], _w3[3];
  cv::Rect_<float> inner1, inner2, outer1, outer2;

  CvMat om  = cvMat(3, 1, CV_64F, _om);
  CvMat t   = cvMat(3, 1, CV_64F, _t);
  CvMat uu  = cvMat(3, 1, CV_64F, _uu);
  CvMat r_r = cvMat(3, 3, CV_64F, _r_r);
  CvMat pp  = cvMat(3, 4, CV_64F, _pp);
  CvMat ww  = cvMat(3, 1, CV_64F, _ww);  // temps
  CvMat w3  = cvMat(3, 1, CV_64F, _w3);  // temps
  CvMat wR  = cvMat(3, 3, CV_64F, _wr);
  CvMat Z   = cvMat(3, 1, CV_64F, _z);
  CvMat Ri  = cvMat(3, 3, CV_64F, _ri);
  double nx = imageSize.width, ny = imageSize.height;
  int i, k;
  double nt, nw;
  if ( matR->rows == 3 && matR->cols == 3)
      cvRodrigues2(matR, &om);          // get vector rotation
  else
      cvConvert(matR, &om);  // it's already a rotation vector
  cvConvertScale(&om, &om, -0.5);  // get average rotation
  cvRodrigues2(&om, &r_r);  // rotate cameras to same orientation by averaging
  cvMatMul(&r_r, matT, &t);
  int idx = fabs(_t[0]) > fabs(_t[1]) ? 0 : 1;

  // if idx == 0
  //   e1 = T / ||T||
  //   e2 = e1 x [0,0,1]

  // if idx == 1
  //   e2 = T / ||T||
  //   e1 = e2 x [0,0,1]

  // e3 = e1 x e2

  _uu[2] = 1;
  cvCrossProduct(&uu, &t, &ww);
  nt = cvNorm(&t, 0, CV_L2);
  nw = cvNorm(&ww, 0, CV_L2);
  cvConvertScale(&ww, &ww, 1 / nw);
  cvCrossProduct(&t, &ww, &w3);
  nw = cvNorm(&w3, 0, CV_L2);
  cvConvertScale(&w3, &w3, 1 / nw);
  _uu[2] = 0;

  for (i = 0; i < 3; ++i) {
      _wr[idx][i] = -_t[i] / nt;
      _wr[idx ^ 1][i] = -_ww[i];
      _wr[2][i] = _w3[i] * (1 - 2 * idx);  // if idx == 1 -> opposite direction
  }

  // apply to both views
  cvGEMM(&wR, &r_r, 1, 0, 0, &Ri, CV_GEMM_B_T);
  cvConvert(&Ri, _R1);
  cvGEMM(&wR, &r_r, 1, 0, 0, &Ri, 0);
  cvConvert(&Ri, _R2);
  cvMatMul(&Ri, matT, &t);

  // calculate projection/camera matrices
  // these contain the relevant rectified image internal params (fx, fy=fx, cx, cy)
  double fc_new = DBL_MAX;
  CvPoint2D64f cc_new[2] = {{0, 0}, {0, 0}};
  newImgSize = newImgSize.width * newImgSize.height != 0 ?
      newImgSize : imageSize;
  const double ratio_x = static_cast<double>(newImgSize.width) /
      imageSize.width / 2;
  const double ratio_y = static_cast<double>(newImgSize.height) /
      imageSize.height / 2;
  const double ratio = idx == 1 ? ratio_x : ratio_y;
  fc_new = (cvmGet(K1, idx ^ 1, idx ^ 1) +
      cvmGet(K2, idx ^ 1, idx ^ 1)) * ratio;

  for (k = 0; k < 2; k++) {
    CvPoint2D32f _pts[4];
    CvPoint3D32f _pts_3[4];
    CvMat pts = cvMat(1, 4, CV_32FC2, _pts);
    CvMat pts_3 = cvMat(1, 4, CV_32FC3, _pts_3);
    // Eigen::Vector2d a;
    // Eigen::Vector3d b;
    models::Vector2d a(2, 1);
    models::Vector3d b(3, 1);
    for (i = 0; i < 4; i++) {
      int j = (i < 2) ? 0 : 1;
      a(0) = static_cast<float>((i % 2)*(nx));
      a(1) = static_cast<float>(j*(ny));
      if (0 == k) {
        leftOdo->liftProjective(a, b);
      } else {
        rightOdo->liftProjective(a, b);
      }
      _pts[i].x = b(0)/b(2);
      _pts[i].y = b(1)/b(2);
    }
    cvConvertPointsHomogeneous(&pts, &pts_3);

    // Change camera matrix to have cc=[0,0] and fc = fc_new
    double _a_tmp[3][3];
    CvMat A_tmp  = cvMat(3, 3, CV_64F, _a_tmp);
    _a_tmp[0][0] = fc_new;
    _a_tmp[1][1] = fc_new;
    _a_tmp[0][2] = 0.0;
    _a_tmp[1][2] = 0.0;
    cvProjectPoints2(&pts_3, k == 0 ? _R1 : _R2, &Z, &A_tmp, 0, &pts);
    CvScalar avg = cvAvg(&pts);
    cc_new[k].x = (nx)/2 - avg.val[0];
    cc_new[k].y = (ny)/2 - avg.val[1];
  }

  if (flags & cv::CALIB_ZERO_DISPARITY) {
    cc_new[0].x = cc_new[1].x = (cc_new[0].x + cc_new[1].x)*0.5;
    cc_new[0].y = cc_new[1].y = (cc_new[0].y + cc_new[1].y)*0.5;
  } else if (idx == 0) {
    // horizontal stereo
    cc_new[0].y = cc_new[1].y = (cc_new[0].y + cc_new[1].y)*0.5;
  } else {
    // vertical stereo
    cc_new[0].x = cc_new[1].x = (cc_new[0].x + cc_new[1].x)*0.5;
  }

  cvZero(&pp);
  _pp[0][0] = _pp[1][1] = fc_new;
  _pp[0][2] = cc_new[0].x;
  _pp[1][2] = cc_new[0].y;
  _pp[2][2] = 1;
  cvConvert(&pp, _P1);

  _pp[0][2] = cc_new[1].x;
  _pp[1][2] = cc_new[1].y;
  _pp[idx][3] = _t[idx]*fc_new;  // baseline * focal length
  *T_mul_f = 0. - _t[idx] * fc_new;
  cvConvert(&pp, _P2);

  _alpha = MIN(alpha, 1.);
  {
    newImgSize = newImgSize.width*newImgSize.height != 0 ?
        newImgSize : imageSize;
    double cx1_0 = cc_new[0].x;
    double cy1_0 = cc_new[0].y;
    double cx2_0 = cc_new[1].x;
    double cy2_0 = cc_new[1].y;
    double cx1 = newImgSize.width*cx1_0/imageSize.width;
    double cy1 = newImgSize.height*cy1_0/imageSize.height;
    double cx2 = newImgSize.width*cx2_0/imageSize.width;
    double cy2 = newImgSize.height*cy2_0/imageSize.height;
    double s = 1.;

    if ( _alpha >= 0 ) {
      double s0 = std::max(std::max(std::max((double)cx1/(cx1_0 - inner1.x), (double)cy1/(cy1_0 - inner1.y)),
                          (double)(newImgSize.width - cx1)/(inner1.x + inner1.width - cx1_0)),
                      (double)(newImgSize.height - cy1)/(inner1.y + inner1.height - cy1_0));
      s0 = std::max(std::max(std::max(std::max((double)cx2/(cx2_0 - inner2.x), (double)cy2/(cy2_0 - inner2.y)),
                        (double)(newImgSize.width - cx2)/(inner2.x + inner2.width - cx2_0)),
                    (double)(newImgSize.height - cy2)/(inner2.y + inner2.height - cy2_0)),
                s0); 

      double s1 = std::min(std::min(std::min((double)cx1/(cx1_0 - outer1.x), (double)cy1/(cy1_0 - outer1.y)),
                          (double)(newImgSize.width - cx1)/(outer1.x + outer1.width - cx1_0)),
                      (double)(newImgSize.height - cy1)/(outer1.y + outer1.height - cy1_0));
      s1 = std::min(std::min(std::min(std::min((double)cx2/(cx2_0 - outer2.x), (double)cy2/(cy2_0 - outer2.y)),
                        (double)(newImgSize.width - cx2)/(outer2.x + outer2.width - cx2_0)),
                    (double)(newImgSize.height - cy2)/(outer2.y + outer2.height - cy2_0)),
                s1);
      s = s0*(1 - alpha) + s1*alpha;
    }

    fc_new *= s;
    cc_new[0] = cvPoint2D64f(cx1, cy1);
    cc_new[1] = cvPoint2D64f(cx2, cy2);

    cvmSet(_P1, 0, 0, fc_new);
    cvmSet(_P1, 1, 1, fc_new);
    cvmSet(_P1, 0, 2, cx1);
    cvmSet(_P1, 1, 2, cy1);

    cvmSet(_P2, 0, 0, fc_new);
    cvmSet(_P2, 1, 1, fc_new);
    cvmSet(_P2, 0, 2, cx2);
    cvmSet(_P2, 1, 2, cy2);
    cvmSet(_P2, idx, 3, s*cvmGet(_P2, idx, 3));

    *cx1_min_cx2 = -(cx1 - cx2);
    // std::cout << "info_pair.T_mul_f :" << *T_mul_f << std::endl;
    // std::cout << "info_pair.cx1_minus_cx2 :" << *cx1_min_cx2 << std::endl;
  }
}
*/

void RectifyProcessor::stereoRectify1(
    models::CameraPtr leftOdo,
    models::CameraPtr rightOdo, 
    const cv::Mat& K1, 
    const cv::Mat& K2,
    const cv::Mat& D1, 
    const cv::Mat& D2, 
    const cv::Size& imageSize,
    const cv::Mat& matR, 
    const cv::Mat& matT,
    cv::Mat& _R1, 
    cv::Mat& _R2, 
    cv::Mat& _P1, 
    cv::Mat& _P2, 
    cv::Size& newImgSize,
    double& T_mul_f,
    double& cx1_min_cx2,
    int flags, 
    double alpha) 
{
  // std::cout << _alpha << std::endl;
  alpha = _alpha;
  cv::Rect_<float> inner1, inner2, outer1, outer2;

  // Create some Matx mirror-storage to play with...
  cv::Matx<double, 3, 3> R1, R2;
  const cv::Vec<double, 3> T(matT.at<double>(0, 0), matT.at<double>(1, 0), matT.at<double>(2, 0) );
  
  cv::Vec<double, 3> om;
  cv::Vec<double, 3> t(0, 0, 0);
  cv::Vec<double, 3> uu(0, 0, 0);
  cv::Matx<double, 3, 3> r_r;
  cv::Matx<double, 3, 4> pp;
  cv::Vec<double, 3> ww;  // temps
  cv::Vec<double, 3> w3;  // temps
  cv::Matx<double, 3, 3> wR;
  cv::Vec<double, 3> Z(0, 0, 0);
  cv::Matx<double, 3, 3> Ri;
  
  double nx = imageSize.width, ny = imageSize.height;
  int i, k;
  double nt, nw;
  if ( matR.rows == 3 && matR.cols == 3)
  {
      cv::Rodrigues(matR, om);          // get vector rotation
  }
  else
  {
        //cvConvert(matR, &om);  // it's already a rotation vector
        om[0] = matR.at<double>(0, 0);
        om[1] = matR.at<double>(1, 0);
        om[2] = matR.at<double>(2, 0);
  }
  
  //cvConvertScale(&om, &om, -0.5);  // get average rotation
  om *= -0.5;
  //cvRodrigues2(&om, &r_r);  // rotate cameras to same orientation by averaging
  cv::Rodrigues(om, r_r);
  //cvMatMul(&r_r, matT, &t);
  t = r_r*T;
  
  int idx = fabs(t[0]) > fabs(t[1]) ? 0 : 1;

  // if idx == 0
  //   e1 = T / ||T||
  //   e2 = e1 x [0,0,1]

  // if idx == 1
  //   e2 = T / ||T||
  //   e1 = e2 x [0,0,1]

  // e3 = e1 x e2

  uu[2] = 1;
  //cvCrossProduct(&uu, &t, &ww);
  ww[0] = -uu[2]*t[1] + uu[1]*t[2];
  ww[1] =  uu[2]*t[0] - uu[0]*t[2];
  ww[2] = -uu[1]*t[0] + uu[0]*t[1];
  
  //nt = cvNorm(&t, 0, CV_L2);
  //nw = cvNorm(&ww, 0, CV_L2);
  nt = cv::norm(t);
  nw = cv::norm(ww);
  
  //cvConvertScale(&ww, &ww, 1 / nw);
  ww *= 1.0 / nw;
  //cvCrossProduct(&t, &ww, &w3);
  w3[0] = -t[2]*ww[1] + t[1]*ww[2];
  w3[1] =  t[2]*ww[0] - t[0]*ww[2];
  w3[2] = -t[1]*ww[0] + t[0]*ww[1];
  
  //nw = cvNorm(&w3, 0, CV_L2);
  //cvConvertScale(&w3, &w3, 1 / nw);
  nw = cv::norm(w3);
  w3 *= 1.0/ nw;
  
  uu[2] = 0;

  for (i = 0; i < 3; ++i) 
  {
      wR(idx, i) = -t[i] / nt;
      wR(idx ^ 1, i) = -ww[i];
      wR(2, i) = w3[i] * (1 - 2 * idx);  // if idx == 1 -> opposite direction
  }

  // apply to both views
  //cvGEMM(&wR, &r_r, 1, 0, 0, &Ri, CV_GEMM_B_T);
  //cvConvert(&Ri, _R1);
  Ri = wR * r_r.t();
  R1 = Ri;
  _R1 = cv::Mat(R1);
  
  //cvGEMM(&wR, &r_r, 1, 0, 0, &Ri, 0);
  //cvConvert(&Ri, _R2);
  //cvMatMul(&Ri, matT, &t);
  Ri = wR.t()*r_r;
  R2 = Ri;
  _R2 = cv::Mat(Ri);
  t = Ri*T;
  
  // calculate projection/camera matrices
  // these contain the relevant rectified image internal params (fx, fy=fx, cx, cy)
  double fc_new = DBL_MAX;
  //CvPoint2D64f cc_new[2] = {{0, 0}, {0, 0}};
  cv::Point_<double> cc_new[2] = {{0, 0}, {0, 0}};
  
  newImgSize = newImgSize.width * newImgSize.height != 0 ? newImgSize : imageSize;
  
  const double ratio_x = static_cast<double>(newImgSize.width) /
      imageSize.width / 2;
  const double ratio_y = static_cast<double>(newImgSize.height) /
      imageSize.height / 2;
  const double ratio = idx == 1 ? ratio_x : ratio_y;
  //fc_new = (cvmGet(K1, idx ^ 1, idx ^ 1) + cvmGet(K2, idx ^ 1, idx ^ 1)) * ratio;
  fc_new = ( K1.at<double>(idx ^ 1, idx ^ 1) + K2.at<double>(idx ^ 1, idx ^ 1) )*ratio;
  
  for (k = 0; k < 2; k++) 
  {
    //CvPoint2D32f _pts[4];
    //CvPoint3D32f _pts_3[4];
    //CvMat pts = cvMat(1, 4, CV_32FC2, _pts);
    //CvMat pts_3 = cvMat(1, 4, CV_32FC3, _pts_3);
    std::vector<cv::Point_<float>> pts; pts.resize(4);
    std::vector<cv::Point3_<float>> pts_3; pts_3.resize(4);
    //CvMat pts = cvMat(1, 4, CV_32FC2, _pts);
    //CvMat pts_3 = cvMat(1, 4, CV_32FC3, _pts_3);
    
    // Eigen::Vector2d a;
    // Eigen::Vector3d b;
    models::Vector2d a(2, 1);
    models::Vector3d b(3, 1);
    
    for (i = 0; i < 4; i++) {
      int j = (i < 2) ? 0 : 1;
      a(0) = static_cast<float>((i % 2)*(nx));
      a(1) = static_cast<float>(j*(ny));
      if (0 == k) {
        leftOdo->liftProjective(a, b);
      } else {
        rightOdo->liftProjective(a, b);
      }
      pts[i].x = pts_3[i].x = b(0)/b(2);
      pts[i].y = pts_3[i].y = b(1)/b(2);
      pts_3[i].z = 1;
    }
    //cvConvertPointsHomogeneous(&pts, &pts_3);

    // Change camera matrix to have cc=[0,0] and fc = fc_new
    //double _a_tmp[3][3];
    //CvMat A_tmp  = cvMat(3, 3, CV_64F, _a_tmp);
    //_a_tmp[0][0] = fc_new;
    //_a_tmp[1][1] = fc_new;
    //_a_tmp[0][2] = 0.0;
    //_a_tmp[1][2] = 0.0;
    cv::Matx<double, 3, 3> A_tmp(fc_new, 0, 0, 
                                 0,  fc_new, 0, 
                                 0, 0, 1);
    //cvProjectPoints2(&pts_3, k == 0 ? _R1 : _R2, &Z, &A_tmp, 0, &pts);
    cv::projectPoints(pts_3, k == 0 ? R1 : R2, Z, A_tmp, 0, pts);
    //CvScalar avg = cvAvg(&pts);
    cv::Point_<float> avg(0.25*(pts[0].x + pts[1].x + pts[2].x + pts[3].x), 
                          0.25*(pts[0].y + pts[1].y + pts[2].y + pts[3].y));
    //cc_new[k].x = (nx)/2 - avg.val[0];
    //cc_new[k].y = (ny)/2 - avg.val[1];
    cc_new[k].x = (nx)/2 - avg.x;
    cc_new[k].y = (ny)/2 - avg.y;
  }

  if (flags & cv::CALIB_ZERO_DISPARITY) {
    cc_new[0].x = cc_new[1].x = (cc_new[0].x + cc_new[1].x)*0.5;
    cc_new[0].y = cc_new[1].y = (cc_new[0].y + cc_new[1].y)*0.5;
  } else if (idx == 0) {
    // horizontal stereo
    cc_new[0].y = cc_new[1].y = (cc_new[0].y + cc_new[1].y)*0.5;
  } else {
    // vertical stereo
    cc_new[0].x = cc_new[1].x = (cc_new[0].x + cc_new[1].x)*0.5;
  }

  //cvZero(&pp);
  //_pp[0][0] = _pp[1][1] = fc_new;
  //_pp[0][2] = cc_new[0].x;
  //_pp[1][2] = cc_new[0].y;
  //_pp[2][2] = 1;
  //cvConvert(&pp, _P1);
  pp = cv::Matx<double, 3, 4>::zeros();
  pp(0, 0) = pp(1, 1) = fc_new;  pp(0, 2) = cc_new[0].x;
  pp(1, 0) = 0;                  pp(1, 2) = cc_new[0].y;
  pp(2, 2) = 1; 
  _P1 = cv::Mat(pp);

  //_pp[0][2] = cc_new[1].x;
  //_pp[1][2] = cc_new[1].y;
  //_pp[idx][3] = _t[idx]*fc_new;  // baseline * focal length
  //*T_mul_f = 0. - _t[idx] * fc_new;
  //cvConvert(&pp, _P2);
  pp(0, 2) = cc_new[1].x;
  pp(1, 2) = cc_new[1].y;
  pp(idx, 3) = t[idx]*fc_new;  // baseline * focal length
  T_mul_f = 0. - t[idx] * fc_new;
  _P2 = cv::Mat(pp);

  _alpha = MIN(alpha, 1.);
  {
    newImgSize = newImgSize.width*newImgSize.height != 0 ?
        newImgSize : imageSize;
    double cx1_0 = cc_new[0].x;
    double cy1_0 = cc_new[0].y;
    double cx2_0 = cc_new[1].x;
    double cy2_0 = cc_new[1].y;
    double cx1 = newImgSize.width*cx1_0/imageSize.width;
    double cy1 = newImgSize.height*cy1_0/imageSize.height;
    double cx2 = newImgSize.width*cx2_0/imageSize.width;
    double cy2 = newImgSize.height*cy2_0/imageSize.height;
    double s = 1.;

    if ( _alpha >= 0 ) {
      double s0 = std::max(std::max(std::max((double)cx1/(cx1_0 - inner1.x), (double)cy1/(cy1_0 - inner1.y)),
                          (double)(newImgSize.width - cx1)/(inner1.x + inner1.width - cx1_0)),
                      (double)(newImgSize.height - cy1)/(inner1.y + inner1.height - cy1_0));
      s0 = std::max(std::max(std::max(std::max((double)cx2/(cx2_0 - inner2.x), (double)cy2/(cy2_0 - inner2.y)),
                        (double)(newImgSize.width - cx2)/(inner2.x + inner2.width - cx2_0)),
                    (double)(newImgSize.height - cy2)/(inner2.y + inner2.height - cy2_0)),
                s0); 

      double s1 = std::min(std::min(std::min((double)cx1/(cx1_0 - outer1.x), (double)cy1/(cy1_0 - outer1.y)),
                          (double)(newImgSize.width - cx1)/(outer1.x + outer1.width - cx1_0)),
                      (double)(newImgSize.height - cy1)/(outer1.y + outer1.height - cy1_0));
      s1 = std::min(std::min(std::min(std::min((double)cx2/(cx2_0 - outer2.x), (double)cy2/(cy2_0 - outer2.y)),
                        (double)(newImgSize.width - cx2)/(outer2.x + outer2.width - cx2_0)),
                    (double)(newImgSize.height - cy2)/(outer2.y + outer2.height - cy2_0)),
                s1);
      s = s0*(1 - alpha) + s1*alpha;
    }

    fc_new *= s;
    //cc_new[0] = cvPoint2D64f(cx1, cy1);
    //cc_new[1] = cvPoint2D64f(cx2, cy2);
    cc_new[0] = cv::Point_<double>(cx1, cy1);
    cc_new[1] = cv::Point_<double>(cx2, cy2);

    //cvmSet(_P1, 0, 0, fc_new);
    //cvmSet(_P1, 1, 1, fc_new);
    //cvmSet(_P1, 0, 2, cx1);
    //cvmSet(_P1, 1, 2, cy1);
    _P1.at<double>(0, 0) = fc_new;
    _P1.at<double>(1, 1) = fc_new;
    _P1.at<double>(0, 2) = cx1;
    _P1.at<double>(1, 2) = cy1;
    
    //cvmSet(_P2, 0, 0, fc_new);
    //cvmSet(_P2, 1, 1, fc_new);
    //cvmSet(_P2, 0, 2, cx2);
    //cvmSet(_P2, 1, 2, cy2);
    //cvmSet(_P2, idx, 3, s*cvmGet(_P2, idx, 3));
    
    _P2.at<double>(0, 0) = fc_new;
    _P2.at<double>(1, 1) = fc_new;
    
    _P2.at<double>(0, 2) = cx2;
    _P2.at<double>(1, 2) = cy2;
    _P2.at<double>(idx, 3) = s*_P2.at<double>(idx, 3);
    
    //*cx1_min_cx2 = -(cx1 - cx2);
    cx1_min_cx2 = -(cx1 - cx2);
    // std::cout << "info_pair.T_mul_f :" << *T_mul_f << std::endl;
    // std::cout << "info_pair.cx1_minus_cx2 :" << *cx1_min_cx2 << std::endl;
  }
}


// Eigen::Matrix4d RectifyProcessor::loadT(const mynteye::Extrinsics& in) {
  // subEigen
models::Matrix4d RectifyProcessor::loadT(const mynteye::Extrinsics &in) {
  models::Matrix3d R(3);
  R<<
  in.rotation[0][0] << in.rotation[0][1] << in.rotation[0][2] <<
  in.rotation[1][0] << in.rotation[1][1] << in.rotation[1][2] <<
  in.rotation[2][0] << in.rotation[2][1] << in.rotation[2][2];

  double t_x = in.translation[0];
  double t_y = in.translation[1];
  double t_z = in.translation[2];

  models::Quaterniond q(R);
  q.normalize();
  models::Matrix4d T(4);
  T(3, 3) = 1;
  T.topLeftCorner<3, 3>() = q.toRotationMatrix();
  models::Vector3d t(3, 1);
  t << t_x << t_y << t_z;
  T.topRightCorner<3, 1>() = t;

  return T;
}

void RectifyProcessor::loadCameraMatrix(cv::Mat& K, cv::Mat& D,  // NOLINT
    cv::Size& image_size,  // NOLINT
    struct CameraROSMsgInfo& calib_data) {  // NOLINT
  K = cv::Mat(3, 3, CV_64F, calib_data.K);
  std::size_t d_length = 4;
  D = cv::Mat(1, d_length, CV_64F, calib_data.D);
  image_size = cv::Size(calib_data.width, calib_data.height);
}

struct CameraROSMsgInfo RectifyProcessor::getCalibMatData(
    const mynteye::IntrinsicsEquidistant& in) {
  struct CameraROSMsgInfo calib_mat_data;
  calib_mat_data.distortion_model = "KANNALA_BRANDT";
  calib_mat_data.height = in.height;
  calib_mat_data.width = in.width;

  for (unsigned int i = 0; i < 4; i++) {
    calib_mat_data.D[i] = in.coeffs[i];
  }

  calib_mat_data.K[0] = in.coeffs[4];  // mu
  calib_mat_data.K[4] = in.coeffs[5];  // mv();
  calib_mat_data.K[2] = in.coeffs[6];  // u0();
  calib_mat_data.K[5] = in.coeffs[7];  // v0();
  calib_mat_data.K[8] = 1;
  return calib_mat_data;
}

std::shared_ptr<struct CameraROSMsgInfoPair> RectifyProcessor::stereoRectify(
    models::CameraPtr leftOdo,
    models::CameraPtr rightOdo,
    mynteye::IntrinsicsEquidistant in_left,
    mynteye::IntrinsicsEquidistant in_right,
    mynteye::Extrinsics ex_right_to_left) {
  // Eigen::Matrix4d T = loadT(ex_right_to_left);
  // Eigen::Matrix3d R = T.topLeftCorner<3, 3>();
  // Eigen::Vector3d t = T.topRightCorner<3, 1>();
  models::Matrix4d T = loadT(ex_right_to_left);
  models::Matrix3d R;
  R = T.topLeftCorner<3, 3>();
  models::Vector3d t = T.topRightCorner<3, 1>();
  // cv::Mat cv_R, cv_t;
  // cv::eigen2cv(R, cv_R);
  cv::Mat cv_R(3, 3, CV_64FC1);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      cv_R.at<double>(i, j) = R(i, j);
    }
  }
  // cv::eigen2cv(t, cv_t);
  cv::Mat cv_t(3, 1, CV_64FC1);
  for (int i = 0; i < 3; ++i) {
      cv_t.at<double>(i, 0) = t(i, 0);
  }
  cv::Mat K1, D1, K2, D2;
  cv::Size image_size1, image_size2;

  struct CameraROSMsgInfo calib_mat_data_left = getCalibMatData(in_left);
  struct CameraROSMsgInfo calib_mat_data_right = getCalibMatData(in_right);

  loadCameraMatrix(K1, D1, image_size1, calib_mat_data_left);
  loadCameraMatrix(K2, D2, image_size2, calib_mat_data_right);

  cv::Mat R1 = cv::Mat(cv::Size(3, 3), CV_64F);
  cv::Mat R2 = cv::Mat(cv::Size(3, 3), CV_64F);
  cv::Mat P1 = cv::Mat(3, 4, CV_64F);
  cv::Mat P2 = cv::Mat(3, 4, CV_64F);

  //CvMat c_R = cv_R, c_t = cv_t;
  //CvMat c_K1 = K1, c_K2 = K2, c_D1 = D1, c_D2 = D2;
  //CvMat c_R1 = R1, c_R2 = R2, c_P1 = P1, c_P2 = P2;
  double T_mul_f;
  double cx1_min_cx2;
  //stereoRectify(leftOdo, rightOdo, &c_K1, &c_K2, &c_D1, &c_D2,
  //    image_size1, &c_R, &c_t, &c_R1, &c_R2, &c_P1, &c_P2, &T_mul_f,
  //    &cx1_min_cx2);
  cv::Size newImgSize = cv::Size(0, 0);
  stereoRectify1(leftOdo, rightOdo, K1, K2, D1, D2,
        image_size1, cv_R, cv_t, R1, R2, P1, P2, newImgSize,
        T_mul_f,cx1_min_cx2);

#ifdef _DOUTPUT
  std::cout << "K1: " << K1 << std::endl;
  std::cout << "D1: " << D1 << std::endl;
  std::cout << "K2: " << K2 << std::endl;
  std::cout << "D2: " << D2 << std::endl;
  std::cout << "R: " << cv_R << std::endl;
  std::cout << "t: " << cv_t << std::endl;
  std::cout << "R1: " << R1 << std::endl;
  std::cout << "R2: " << R2 << std::endl;
  std::cout << "P1: " << P1 << std::endl;
  std::cout << "P2: " << P2 << std::endl;
#endif
  R1 = rectifyrad(R1);
  R2 = rectifyrad(R2);

  for (std::size_t i = 0; i < 3; i++) {
    for (std::size_t j = 0; j < 4; j++) {
      calib_mat_data_left.P[i*4 + j] = P1.at<double>(i, j);
      calib_mat_data_right.P[i*4 + j] = P2.at<double>(i, j);
    }
  }

  for (std::size_t i = 0; i < 3; i++) {
    for (std::size_t j = 0; j < 3; j++) {
      calib_mat_data_left.R[i*3 + j] = R1.at<double>(i, j);
      calib_mat_data_right.R[i*3 +j] = R2.at<double>(i, j);
    }
  }

  struct CameraROSMsgInfoPair info_pair;
  info_pair.left = calib_mat_data_left;
  info_pair.right = calib_mat_data_right;
  info_pair.T_mul_f = T_mul_f;
  info_pair.cx1_minus_cx2 = cx1_min_cx2;
  for (std::size_t i = 0; i< 3 * 4; i++) {
    info_pair.P[i] = calib_mat_data_left.P[i];
  }

  info_pair.R[0] = ex_right_to_left.rotation[0][0];
  info_pair.R[1] = ex_right_to_left.rotation[0][1];
  info_pair.R[2] = ex_right_to_left.rotation[0][2];
  info_pair.R[3] = ex_right_to_left.rotation[1][0];
  info_pair.R[4] = ex_right_to_left.rotation[1][1];
  info_pair.R[5] = ex_right_to_left.rotation[1][2];
  info_pair.R[6] = ex_right_to_left.rotation[2][0];
  info_pair.R[7] = ex_right_to_left.rotation[2][1];
  info_pair.R[8] = ex_right_to_left.rotation[2][2];

  return std::make_shared<struct CameraROSMsgInfoPair>(info_pair);
}

models::CameraPtr RectifyProcessor::generateCameraFromIntrinsicsEquidistant(
    const mynteye::IntrinsicsEquidistant & in) {
  models::EquidistantCameraPtr camera(
      new models::EquidistantCamera("KANNALA_BRANDT",
                                       in.width,
                                       in.height,
                                       in.coeffs[0],
                                       in.coeffs[1],
                                       in.coeffs[2],
                                       in.coeffs[3],
                                       in.coeffs[4],
                                       in.coeffs[5],
                                       in.coeffs[6],
                                       in.coeffs[7]));
  return camera;
}

void RectifyProcessor::InitParams(
    IntrinsicsEquidistant in_left,
    IntrinsicsEquidistant in_right,
    Extrinsics ex_right_to_left) {
  calib_model = CalibrationModel::KANNALA_BRANDT;
  in_left.ResizeIntrinsics();
  in_right.ResizeIntrinsics();
  in_left_cur = in_left;
  in_right_cur = in_right;
  ex_right_to_left_cur = ex_right_to_left;

  models::CameraPtr camera_odo_ptr_left =
      generateCameraFromIntrinsicsEquidistant(in_left);
  models::CameraPtr camera_odo_ptr_right =
      generateCameraFromIntrinsicsEquidistant(in_right);

  auto calib_info_tmp = stereoRectify(camera_odo_ptr_left,
        camera_odo_ptr_right,
        in_left,
        in_right,
        ex_right_to_left);

  *calib_infos = *calib_info_tmp;
  cv::Mat rect_R_l =
      cv::Mat::eye(3, 3, CV_32F), rect_R_r = cv::Mat::eye(3, 3, CV_32F);
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      rect_R_l.at<float>(i, j) = calib_infos->left.R[i*3+j];
      rect_R_r.at<float>(i, j) = calib_infos->right.R[i*3+j];
    }
  }

  double left_f[] =
      {calib_infos->left.P[0], calib_infos->left.P[5]};
  double left_center[] =
      {calib_infos->left.P[2], calib_infos->left.P[6]};
  double right_f[] =
      {calib_infos->right.P[0], calib_infos->right.P[5]};
  double right_center[] =
      {calib_infos->right.P[2], calib_infos->right.P[6]};

  camera_odo_ptr_left->initUndistortRectifyMap(
      map11, map12, left_f[0], left_f[1],
      cv::Size(0, 0), left_center[0],
      left_center[1], rect_R_l);
  camera_odo_ptr_right->initUndistortRectifyMap(
      map21, map22, right_f[0], right_f[1],
      cv::Size(0, 0), right_center[0],
      right_center[1], rect_R_r);
}

const char RectifyProcessor::NAME[] = "RectifyProcessor";

RectifyProcessor::RectifyProcessor(
      std::shared_ptr<IntrinsicsBase> intr_left,
      std::shared_ptr<IntrinsicsBase> intr_right,
      std::shared_ptr<Extrinsics> extr,
      std::int32_t proc_period)
    : Processor(std::move(proc_period)),
      calib_model(CalibrationModel::UNKNOW),
      _alpha(-1) {

  calib_infos = std::make_shared<struct CameraROSMsgInfoPair>();
  InitParams(
    *std::dynamic_pointer_cast<IntrinsicsEquidistant>(intr_left),
    *std::dynamic_pointer_cast<IntrinsicsEquidistant>(intr_right),
    *extr);
}

RectifyProcessor::~RectifyProcessor() {
  VLOG(2) << __func__;
}

std::string RectifyProcessor::Name() {
  return NAME;
}

void RectifyProcessor::ReloadImageParams(
      std::shared_ptr<IntrinsicsBase> intr_left,
      std::shared_ptr<IntrinsicsBase> intr_right,
      std::shared_ptr<Extrinsics> extr) {
  InitParams(
    *std::dynamic_pointer_cast<IntrinsicsEquidistant>(intr_left),
    *std::dynamic_pointer_cast<IntrinsicsEquidistant>(intr_right),
    *extr);
}

Object *RectifyProcessor::OnCreateOutput() {
  return new ObjMat2();
}

bool RectifyProcessor::SetRectifyAlpha(float alpha) {
  _alpha = alpha;
  ReloadImageParams();
  return true;
}

bool RectifyProcessor::OnProcess(
    Object *const in, Object *const out,
    std::shared_ptr<Processor> const parent) {
  MYNTEYE_UNUSED(parent)
  const ObjMat2 *input = Object::Cast<ObjMat2>(in);
  ObjMat2 *output = Object::Cast<ObjMat2>(out);
  cv::remap(input->first, output->first, map11, map12, cv::INTER_LINEAR);
  cv::remap(input->second, output->second, map21, map22, cv::INTER_LINEAR);
  output->first_id = input->first_id;
  output->first_data = input->first_data;
  output->second_id = input->second_id;
  output->second_data = input->second_data;
  return true;
}

MYNTEYE_END_NAMESPACE
