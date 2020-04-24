// Code is based on "Straight forward implementation of a 3d reconstruction
// technique called voxel carving (or space carving)" by xocoatzin.
// https://github.com/xocoatzin/Voxel-Carving
//
// Modifications by: Valentin Yuryev, yuryevv@student.ethz.ch
// Modifications allow for use of GrabCut sillhoutes and COLMAP camera/image
// information created by sparse reconstruction
// Date: 07.06.2017


#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <vtkSmartPointer.h>
#include <vtkStructuredPoints.h>
#include <vtkPointData.h>
#include <vtkPLYWriter.h>
#include <vtkFloatArray.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkMarchingCubes.h>
#include <vtkCleanPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

#include "../include/strtk.hpp"

using namespace cv;
using namespace std;


// Constants for voxel carving
int IMG_WIDTH;
int IMG_HEIGHT;
const int VOXEL_DIM = 256;
const int VOXEL_SIZE = VOXEL_DIM*VOXEL_DIM*VOXEL_DIM;
const int VOXEL_SLICE = VOXEL_DIM*VOXEL_DIM;
const int OUTSIDE = 0;

struct voxel {
  float xpos;
  float ypos;
  float zpos;
  float res;
  float value;
};

struct coord {
  int x;
  int y;
};

struct startParams {
  float startX;
  float startY;
  float startZ;
  float voxelWidth;
  float voxelHeight;
  float voxelDepth;
};

struct camera {
  cv::Mat Image;
  cv::Mat P;
  cv::Mat K;
  cv::Mat R;
  cv::Mat t;
  cv::Mat Silhouette;
};

void exportModel(char *filename, vtkPolyData *polyData) {

  /* exports 3d model in ply format */
  vtkSmartPointer<vtkPLYWriter> plyExporter = vtkSmartPointer<vtkPLYWriter>::New();
  plyExporter->SetFileName(filename);
  plyExporter->SetInput(polyData);
  plyExporter->Update();
  plyExporter->Write();
}

// Projection of points onto 2D image
coord project(camera cam, voxel v) {

  coord im;

  /* project voxel into camera image coords */
  float z =   cam.P.at<float>(2, 0) * v.xpos +
  cam.P.at<float>(2, 1) * v.ypos +
  cam.P.at<float>(2, 2) * v.zpos +
  cam.P.at<float>(2, 3);

  im.y =    (cam.P.at<float>(1, 0) * v.xpos +
  cam.P.at<float>(1, 1) * v.ypos +
  cam.P.at<float>(1, 2) * v.zpos +
  cam.P.at<float>(1, 3)) / z;

  im.x =    (cam.P.at<float>(0, 0) * v.xpos +
  cam.P.at<float>(0, 1) * v.ypos +
  cam.P.at<float>(0, 2) * v.zpos +
  cam.P.at<float>(0, 3)) / z;

  return im;
}

// Model Rendering
void renderModel(float fArray[], startParams params) {

  /* create vtk visualization pipeline from voxel grid (float array) */
  vtkSmartPointer<vtkStructuredPoints> sPoints = vtkSmartPointer<vtkStructuredPoints>::New();
  sPoints->SetDimensions(VOXEL_DIM, VOXEL_DIM, VOXEL_DIM);
  sPoints->SetSpacing(params.voxelDepth, params.voxelHeight, params.voxelWidth);
  sPoints->SetOrigin(params.startZ, params.startY, params.startX);
  sPoints->SetScalarTypeToFloat();

  vtkSmartPointer<vtkFloatArray> vtkFArray = vtkSmartPointer<vtkFloatArray>::New();
  vtkFArray->SetNumberOfValues(VOXEL_SIZE);
  vtkFArray->SetArray(fArray, VOXEL_SIZE, 1);

  sPoints->GetPointData()->SetScalars(vtkFArray);
  sPoints->Update();

  /* create iso surface with marching cubes algorithm */
  vtkSmartPointer<vtkMarchingCubes> mcSource = vtkSmartPointer<vtkMarchingCubes>::New();
  mcSource->SetInputConnection(sPoints->GetProducerPort());
  mcSource->SetNumberOfContours(1);
  mcSource->SetValue(0,0.5);
  mcSource->Update();

  /* recreate mesh topology and merge vertices */
  vtkSmartPointer<vtkCleanPolyData> cleanPolyData = vtkSmartPointer<vtkCleanPolyData>::New();
  cleanPolyData->SetInputConnection(mcSource->GetOutputPort());
  cleanPolyData->Update();

  // Store the vertices into .ply file
  std::string filename = "./output_carved";
  vtkSmartPointer<vtkPLYWriter> plyWriter = vtkSmartPointer<vtkPLYWriter>::New();
  plyWriter->SetFileName(filename.c_str());
  plyWriter->SetInputConnection(cleanPolyData->GetOutputPort());
  plyWriter->Write();
}

// Carving algorithm
void carve(float fArray[], startParams params, camera cam) {

  cv::Mat silhouette, distImage;
  cv::Canny(cam.Silhouette, silhouette, 0, 255);
  cv::bitwise_not(silhouette, silhouette);
  cv::distanceTransform(silhouette, distImage, CV_DIST_L2, 3);
  std::cout << "Carving..." << std::endl;
  for (int i=0; i<VOXEL_DIM; i++) {
    for (int j=0; j<VOXEL_DIM; j++) {
      for (int k=0; k<VOXEL_DIM; k++) {

        /* calc voxel position inside camera view frustum */
        voxel v;
        v.xpos = params.startX + i * params.voxelWidth;
        v.ypos = params.startY + j * params.voxelHeight;
        v.zpos = params.startZ + k * params.voxelDepth;
        v.value = 1.0f;
        coord im = project(cam, v);
        float dist = -1.0f;

        /* test if projected voxel is within image coords */
        if (im.x > 0 && im.y > 0 && im.x < IMG_WIDTH && im.y < IMG_HEIGHT) {
          dist = distImage.at<float>(im.y, im.x);
          if (cam.Silhouette.at<uchar>(im.y, im.x) == OUTSIDE) {
            dist *= -1.0f;
          }
        }
        if (dist < fArray[i*VOXEL_SLICE+j*VOXEL_DIM+k]) {
          fArray[i*VOXEL_SLICE+j*VOXEL_DIM+k] = dist;
        }

      }
    }
  }
}

int main(int argc, char* argv[]) {
  // Instructions on how to use the program
  if (argc < 3)
  {
    std::cout << std::endl << "Instructions:" << std::endl;
    std::cout << "Input 1: Folder Location that contains camrea.txt, images.txt and folder 'sil'. Example: /home/user/folder/" << std::endl;
    std::cout << "Input 2: ID Images in images.txt to look upto. Example: 25 will look up to ID 25" << std::endl;
    std::cout << "Example: ./main /home/user/folder/ 25" << std::endl << std::endl;
    return 0;
  }

  // Read out folder location and number of images to use
  int num_imgs = strtol(argv[2], NULL, 10);
  std::cout << std::endl << "Searching up to ID:  " << num_imgs << std::endl;
  /* acquire camera images, silhouettes and camera matrix */
  std::vector<camera> cameras;
  std::fstream fs;
  std::stringstream folder_loc;
  folder_loc << argv[1] << "cameras.txt";
  fs.open(folder_loc.str().c_str(), std::ios::in);

  std::fstream fs_image_search;
  std::stringstream image_loc;
  image_loc << argv[1] << "images.txt";
  fs_image_search.open(image_loc.str().c_str(),std::ios::in);

  // Read camera information line by line
  std::string line;
  // Parse through the lines and store image ID, size, and camera param
  const char *whitespace    = " \t\r\n\f";
  int count = 0;
  while (std::getline(fs, line ) && count != num_imgs) {
    ++count;

    int ID;
    std::string MODEL;
    int width;
    int height;
    float f;
    float cx;
    float cy;
    float iDontKnow;

    strtk::remove_leading_trailing(whitespace, line);

    // Parse and store camera params
    std::vector<float> floats;
    if( strtk::parse(line, whitespace, floats ) )
    {
      ID = floats[0];
      IMG_WIDTH = floats[1];
      IMG_HEIGHT = floats[2];
      f = floats[3];
      cx = floats[4];
      cy = floats[5];
    }

    int j = 0;
    std::vector<std::string> strings;
    std::string name;
    std::string search_line;
    float qw, qx, qy, qz, x, y, z;

    // Find image corresponding to image id and store its info
    while (std::getline(fs_image_search, search_line )) {
      std::vector<float> floats_search;
      if( strtk::parse(search_line, whitespace, floats_search ) )
      {
        if (floats_search[8] == ID)
        {
          strtk::parse(search_line, whitespace, strings );

          name = strings[9];

          qw = floats_search[1];
          qx = floats_search[2];
          qy = floats_search[3];
          qz = floats_search[4];
          x = floats_search[5];
          y = floats_search[6];
          z = floats_search[7];
          break;
        }
      }
    }

    // Find sillhoute corresponding to the image in the given folder
    std::stringstream sil_loc;
    sil_loc << argv[1] << "sil/" << name;
    // If silhoutte not in the folder continue
    cv::Mat img = cv::imread(sil_loc.str());
    if(! img.data )
    {
        continue;
    }
    // Store camera, transition, rotation parameters
    std::cout << "Processing scene: " << ID << std::endl;
    std::cout << "Image ID: " << ID << " Image Name: " << name << std::endl;

    // Scale silhouette to make sure dark parts are 0 and everything else is 255
    cv::Mat silhouette;
    silhouette = img;
    cv::inRange(silhouette, cv::Scalar(0, 0, 20), cv::Scalar(255,255,255), silhouette);

    std::cout << "Calculating Camera Matrix... " << std::endl;
    cv::Mat P, K, R, t;
    camera c;
    c.Image = img;

    P = cv::Mat::eye(3, 3, CV_32FC1);

    K = cv::Mat::eye(3, 3, CV_32FC1);
    K.at<float>(0,0) = f; /* fx */
    K.at<float>(1,1) = f; /* fy */
    K.at<float>(0,2) = cx; /* cx */
    K.at<float>(1,2) = cy; /* cy */
    c.K = K;
    std::cout << "K: " << c.K << std::endl;

    std::cout << "Adding Rotation Matrix... " << std::endl;
    R = cv::Mat::eye(3, 3, CV_32FC1);
    R.at<float>(0,0) = 1.0f - 2.0f*qy*qy - 2.0f*qz*qz;
    R.at<float>(0,1) = 2.0f*qx*qy - 2.0f*qz*qw;
    R.at<float>(0,2) = 2.0f*qx*qz + 2.0f*qy*qw;
    R.at<float>(1,0) = 2.0f*qx*qy + 2.0f*qz*qw;
    R.at<float>(1,1) = 1.0f - 2.0f*qx*qx - 2.0f*qz*qz;
    R.at<float>(1,2) = 2.0f*qy*qz - 2.0f*qx*qw;
    R.at<float>(2,0) = 2.0f*qx*qz - 2.0f*qy*qw;
    R.at<float>(2,1) = 2.0f*qy*qz + 2.0f*qx*qw;
    R.at<float>(2,2) = 1.0f - 2.0f*qx*qx - 2.0f*qy*qy;
    c.R = R;

    std::cout << "Adding Transition Matrix... " << std::endl;
    t = cv::Mat::ones(3, 1, CV_32FC1);
    t.at<float>(0,0) = x;
    t.at<float>(1,0) = y;
    t.at<float>(2,0) = z;
    c.t = t;

    std::cout << "Adding Camera Matrix... " << std::endl;
    cv::Mat Rt = cv::Mat::ones(3, 4, CV_32FC1);
    Rt.at<float>(0,0) = 1.0f - 2.0f*qy*qy - 2.0f*qz*qz;
    Rt.at<float>(0,1) = 2.0f*qx*qy - 2.0f*qz*qw;
    Rt.at<float>(0,2) = 2.0f*qx*qz + 2.0f*qy*qw;
    Rt.at<float>(1,0) = 2.0f*qx*qy + 2.0f*qz*qw;
    Rt.at<float>(1,1) = 1.0f - 2.0f*qx*qx - 2.0f*qz*qz;
    Rt.at<float>(1,2) = 2.0f*qy*qz - 2.0f*qx*qw;
    Rt.at<float>(2,0) = 2.0f*qx*qz - 2.0f*qy*qw;
    Rt.at<float>(2,1) = 2.0f*qy*qz + 2.0f*qx*qw;
    Rt.at<float>(2,2) = 1.0f - 2.0f*qx*qx - 2.0f*qy*qy;
    Rt.at<float>(0,3) = x;
    Rt.at<float>(1,3) = y;
    Rt.at<float>(2,3) = z;
    std::cout << "Rt: " << Rt << std::endl;
    c.P = K*Rt;

    std::cout << "Adding Silhouette... " << std::endl;
    c.Silhouette = silhouette;

    std::cout << "Adding to Cameras vector... " << std::endl << std::endl;
    cameras.push_back(c);
  }

  std::cout << std::endl << "Beginning Carving" << std::endl;

  // Normally the model is within 2 meter box
  float xmin = -2, ymin = -2, zmin = -2;
  float xmax = 2, ymax = 2, zmax = 2;

  float bbwidth = std::abs(xmax-xmin)*1.15;
  float bbheight = std::abs(ymax-ymin)*1.15;
  float bbdepth = std::abs(zmax-zmin)*1.05;

  startParams params;
  params.startX = xmin-std::abs(xmax-xmin)*0.15;
  params.startY = ymin-std::abs(ymax-ymin)*0.15;
  params.startZ = 0.0f;
  params.voxelWidth = bbwidth/VOXEL_DIM;
  params.voxelHeight = bbheight/VOXEL_DIM;
  params.voxelDepth = bbdepth/VOXEL_DIM;

  /* 3 dimensional voxel grid */
  float *fArray = new float[VOXEL_SIZE];
  std::fill_n(fArray, VOXEL_SIZE, 1000.0f);

  /* carving model for every given camera image */
  for (int i=0; i<cameras.size(); i++) {
    std::cout << "Carving: " << i << std::endl;
    carve(fArray, params, cameras.at(i));
  }

  renderModel(fArray, params);

  return 0;
}
