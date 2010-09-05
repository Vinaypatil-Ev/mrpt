/* +---------------------------------------------------------------------------+
   |          The Mobile Robot Programming Toolkit (MRPT) C++ library          |
   |                                                                           |
   |                   http://mrpt.sourceforge.net/                            |
   |                                                                           |
   |   Copyright (C) 2005-2010  University of Malaga                           |
   |                                                                           |
   |    This software was written by the Machine Perception and Intelligent    |
   |      Robotics Lab, University of Malaga (Spain).                          |
   |    Contact: Jose-Luis Blanco  <jlblanco@ctima.uma.es>                     |
   |                                                                           |
   |  This file is part of the MRPT project.                                   |
   |                                                                           |
   |     MRPT is free software: you can redistribute it and/or modify          |
   |     it under the terms of the GNU General Public License as published by  |
   |     the Free Software Foundation, either version 3 of the License, or     |
   |     (at your option) any later version.                                   |
   |                                                                           |
   |   MRPT is distributed in the hope that it will be useful,                 |
   |     but WITHOUT ANY WARRANTY; without even the implied warranty of        |
   |     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         |
   |     GNU General Public License for more details.                          |
   |                                                                           |
   |     You should have received a copy of the GNU General Public License     |
   |     along with MRPT.  If not, see <http://www.gnu.org/licenses/>.         |
   |                                                                           |
   +---------------------------------------------------------------------------+ */

/* ===========================================================================
    EXAMPLE: bundle_adj_full_demo
    PURPOSE: Demonstrate "mrpt::vision::bundle_adj_full" with a set of
	          simulated or real data. If the program is called without command
			  line arguments, simulated measurements will be used.
			  To use real data, invoke:
			    bundle_adj_full_demo  <feats.txt> <cam_model.cfg>

			  Where <feats.txt> is a "TSequenceFeatureObservations" saved as
			  a text file, and <cam_model.cfg> is a .ini-like file with a
			  section named "CAMERA" loadable by mrpt::utils::TCamera.

    DATE: 20-Aug-2010
   =========================================================================== */

#include <mrpt/vision.h>
#include <mrpt/gui.h>
#include <mrpt/random.h>

using namespace mrpt;
using namespace mrpt::gui;
using namespace mrpt::utils;
using namespace mrpt::math;
using namespace mrpt::opengl;
using namespace std;


vector_double history_avr_err;

// A feedback functor, which is called on each iteration by the optimizer to let us know on the progress:
void my_BundleAdjustmentFeedbackFunctor(
	const size_t cur_iter,
	const double cur_total_sq_error,
	const size_t max_iters,
	const TSequenceFeatureObservations & input_observations,
	const TFramePosesVec & current_frame_estimate,
	const TLandmarkLocationsVec & current_landmark_estimate )
{
	const double avr_err = std::sqrt(cur_total_sq_error)/input_observations.size();
	history_avr_err.push_back(avr_err);
	cout << "[PROGRESS] Iter: " << cur_iter << " avrg err in px: " << avr_err << endl;
	cout.flush();
}


// ------------------------------------------------------
//				bundle_adj_full_demo
// ------------------------------------------------------
void bundle_adj_full_demo(
	const TCamera                        & camera_params,
	const TSequenceFeatureObservations   & allObs,
	TFramePosesVec			& frame_poses,
	TLandmarkLocationsVec   & landmark_points
	)
{
	cout << "Optimizing " << allObs.size() << " feature observations.\n";

	TParametersDouble 		extra_params;
	// extra_params["verbose"] = 1;
	extra_params["max_iterations"]= 250;
	//extra_params["num_fix_frames"] = 1;
	//extra_params["num_fix_points"] = 0;
	extra_params["profiler"] = 1;

	mrpt::vision::bundle_adj_full(
		allObs,
		camera_params,
		frame_poses,
		landmark_points,
		extra_params,
		&my_BundleAdjustmentFeedbackFunctor
		);
}

mrpt::opengl::CSetOfObjectsPtr framePosesVecVisualize(
	const TFramePosesVec &poses,
	const double  len,
	const double  lineWidth);


// ------------------------------------------------------
//						MAIN
// ------------------------------------------------------
int main(int argc, char **argv)
{
	try
	{
		// Simulation or real-data? (read at the top of this file):
		if (argc!=1 && argc!=3)
		{
			cout << "Usage:\n"
				<< argv[0] << "     -> Simulation\n"
				<< argv[0] << " <feats.txt> <cam_model.cfg>\n";
			return 1;
		}

		// BA data:
		TCamera                       camera_params;
		TSequenceFeatureObservations  allObs;
		TFramePosesVec                frame_poses;
		TLandmarkLocationsVec         landmark_points;

		// Only for simulation mode:
		TFramePosesVec                frame_poses_real, frame_poses_noisy; // Ground truth & starting point
		TLandmarkLocationsVec         landmark_points_real, landmark_points_noisy; // Ground truth & starting point

		if (argc==1)
		{
			random::CRandomGenerator rg(1234);

			//  Simulation
			// --------------------------
			// The projective camera model:
			camera_params.ncols = 800;
			camera_params.nrows = 600;
			camera_params.fx(700); camera_params.fy(700);
			camera_params.cx(400); camera_params.cy(300);

			//      Generate synthetic dataset:
			// -------------------------------------
			const size_t nPts = 100;  // # of 3D landmarks
			const double L1 = 60;   // Draw random poses in the rectangle L1xL2xL3
			const double L2 = 10;
			const double L3 = 10;
			const double max_camera_dist = L1*4;

			const double cameraPathLen = L1*1.2;
			//const double cameraPathEllipRadius1 = L1*2;
			//const double cameraPathEllipRadius2 = L2*2;
			// Noise params:
			const double STD_PX_ERROR= 0; //0.10; // pixels
			const double STD_PT3D    = 0.10; // meters
			const double STD_CAM_XYZ = 0.05; // meters
			const double STD_CAM_ANG = DEG2RAD(5); // degs


			landmark_points_real.resize(nPts);
			for (size_t i=0;i<nPts;i++)
			{
				landmark_points_real[i].x = rg.drawUniform(-L1,L1);
				landmark_points_real[i].y = rg.drawUniform(-L2,L2);
				landmark_points_real[i].z = rg.drawUniform(-L3,L3);
			}

			//const double angStep = M_PI*2.0/40;
			const double camPosesSteps = 2*cameraPathLen/20;
			frame_poses_real.clear();

			for (double x=-cameraPathLen;x<cameraPathLen;x+=camPosesSteps)
			{
				TPose3D  p;
				p.x = x; //cameraPathEllipRadius1 * cos(ang);
				p.y = 4*L2;  // cameraPathEllipRadius2 * sin(ang);
				p.z = 0;
				p.yaw = DEG2RAD(-90) - DEG2RAD(30)*x/cameraPathLen  ; // wrapToPi(ang+M_PI);
				p.pitch = 0;
				p.roll  = 0;
				// Angles above is for +X pointing to the (0,0,0), but we want instead +Z pointing there, as typical in camera models:
				frame_poses_real.push_back( CPose3D(p) + CPose3D(0,0,0,DEG2RAD(-90), 0, DEG2RAD(-90)) );
			}

			// Simulate the feature observations:
			allObs.clear();
			map<TCameraPoseID, size_t> numViewedFrom;
			for (size_t i=0;i<frame_poses_real.size();i++) // for each pose
			{
				// predict all landmarks:
				for (size_t j=0;j<landmark_points_real.size();j++)
				{
					TPixelCoordf px = mrpt::vision::pinhole::projectPoint_no_distortion<false>(camera_params, frame_poses_real[i], landmark_points_real[j]);

					px.x += rg.drawGaussian1D(0,STD_PX_ERROR);
					px.y += rg.drawGaussian1D(0,STD_PX_ERROR);

					// Out of image?
					if (px.x<0 || px.y<0 || px.x>camera_params.ncols || px.y>camera_params.nrows)
						continue;

					// Too far?
					const double dist = math::distance( TPoint3D(frame_poses_real[i]), landmark_points_real[j]);
					if (dist>max_camera_dist)
						continue;

					// Ok, accept it:
					allObs.push_back( TFeatureObservation(j,i, px) );
					numViewedFrom[i]++;
				}
			}

			ASSERT_EQUAL_(numViewedFrom.size(), frame_poses_real.size())

			// Make sure all poses and all LMs appear at least once!
			{
				TSequenceFeatureObservations allObs2 = allObs;
				std::map<TCameraPoseID,TCameraPoseID>  old2new_camIDs;
				std::map<TLandmarkID,TLandmarkID>      old2new_lmIDs;
				allObs2.compressIDs(&old2new_camIDs, &old2new_lmIDs);

				ASSERT_EQUAL_(old2new_camIDs.size(),frame_poses_real.size() )
				ASSERT_EQUAL_(old2new_lmIDs.size(), landmark_points_real.size() )
			}


			// Add noise to the data:
			frame_poses_noisy     = frame_poses_real;
			landmark_points_noisy = landmark_points_real;
			for (size_t i=0;i<landmark_points_noisy.size();i++)
				landmark_points_noisy[i] += TPoint3D( rg.drawGaussian1D(0,STD_PT3D),rg.drawGaussian1D(0,STD_PT3D),rg.drawGaussian1D(0,STD_PT3D) );

			for (size_t i=1;i<frame_poses_noisy.size();i++) // DON'T add error to frame[0], the global reference!
			{
				CPose3D bef = frame_poses_noisy[i];
				frame_poses_noisy[i].setFromValues(
					frame_poses_noisy[i].x() + rg.drawGaussian1D(0,STD_CAM_XYZ),
					frame_poses_noisy[i].y() + rg.drawGaussian1D(0,STD_CAM_XYZ),
					frame_poses_noisy[i].z() + rg.drawGaussian1D(0,STD_CAM_XYZ),
					frame_poses_noisy[i].yaw()   + rg.drawGaussian1D(0,STD_CAM_ANG),
					frame_poses_noisy[i].pitch() + rg.drawGaussian1D(0,STD_CAM_ANG),
					frame_poses_noisy[i].roll()  + rg.drawGaussian1D(0,STD_CAM_ANG) );
			}

			// Optimize it:
			frame_poses     = frame_poses_noisy;
			landmark_points = landmark_points_noisy;

			vector<CArrayDouble<2> > resids;
			const double initial_total_sq_err = mrpt::vision::reprojectionResiduals(allObs,camera_params,frame_poses, landmark_points,resids, false);
			cout << "Initial avr error in px: " << std::sqrt(initial_total_sq_err)/allObs.size() << endl;

			bundle_adj_full_demo( camera_params, allObs, frame_poses, landmark_points );
		}
		else
		{
			//  Real data
			// --------------------------
			const string feats_fil = string(argv[1]);
			const string cam_fil   = string(argv[2]);

			cout << "Loading observations from: " << feats_fil << "...";cout.flush();
			allObs.loadFromTextFile(feats_fil);
			cout << "Done.\n";

			//allObs.decimateCameraFrames(5);
			allObs.compressIDs();

			ASSERT_(mrpt::system::fileExists(cam_fil))
			cout << "Loading camera params from: " << cam_fil;
			CConfigFile cfgCam(cam_fil);
			camera_params.loadFromConfigFile("CAMERA",cfgCam);
			cout << "Done.\n";

			cout << "Initial gross estimate...";
			mrpt::vision::ba_initial_estimate(
				allObs,
				camera_params,
				frame_poses,
				landmark_points);
			cout << "OK\n";

			bundle_adj_full_demo( camera_params, allObs, frame_poses, landmark_points );
		}

		// Display results in 3D:
		// -------------------------------
		gui::CDisplayWindow3D  win("Bundle adjustment demo", 800,600);

		COpenGLScenePtr &scene = win.get3DSceneAndLock();


		{	// Ground plane:
			CGridPlaneXYPtr obj = CGridPlaneXY::Create(-200,200,-200,200,0, 5);
			obj->setColor(0.7,0.7,0.7);
			scene->insert(obj);
		}

		if (!landmark_points_real.empty())
		{	// Feature points: ground truth
			CPointCloudPtr obj =  CPointCloud::Create();
			obj->setPointSize(2);
			obj->setColor(0,0,0);
			obj->loadFromPointsList(landmark_points_real);
			scene->insert(obj);
		}
		if (!landmark_points_noisy.empty())
		{	// Feature points: noisy
			CPointCloudPtr obj =  CPointCloud::Create();
			obj->setPointSize(4);
			obj->setColor(0.7,0.2,0.2, 0);
			obj->loadFromPointsList(landmark_points_noisy);
			scene->insert(obj);
		}

		{	// Feature points: estimated
			CPointCloudPtr obj =  CPointCloud::Create();
			obj->setPointSize(3);
			obj->setColor(0,0,1, 1.0);
			obj->loadFromPointsList(landmark_points);
			scene->insert(obj);
		}

		// Camera Frames: estimated
		scene->insert( framePosesVecVisualize(frame_poses_noisy,1.0,  1) );
		scene->insert( framePosesVecVisualize(frame_poses_real,2.0,  1) );
		scene->insert( framePosesVecVisualize(frame_poses,2.0, 3) );


		win.setCameraZoom(100);

		win.unlockAccess3DScene();
		win.repaint();

		// Also, show history of error:
		gui::CDisplayWindowPlots winPlot("Avr error with iterations",500,200);
		//winPlot.setPos(0,620);
		winPlot.plot( history_avr_err, "b.3");
		winPlot.axis_fit();

		cout << "Close the 3D window or press a key to exit.\n";
		win.waitForKey();

		return 0;
	} catch (std::exception &e)
	{
		std::cout << "MRPT exception caught: " << e.what() << std::endl;
		return -1;
	}
	catch (...)
	{
		printf("Untyped exception!!");
		return -1;
	}
}


mrpt::opengl::CSetOfObjectsPtr framePosesVecVisualize(
	const TFramePosesVec &poses,
	const double  len,
	const double  lineWidth)
{
	mrpt::opengl::CSetOfObjectsPtr obj = mrpt::opengl::CSetOfObjects::Create();

	for (size_t i=0;i<poses.size();i++)
	{
		CSetOfObjectsPtr corner = opengl::stock_objects::CornerXYZSimple(len, lineWidth);
		corner->setPose(poses[i]);
		corner->setName(format("%u",(unsigned int)i ));
		corner->enableShowName();
		obj->insert(corner);
	}
	return obj;
}

