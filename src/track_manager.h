#pragma once
#include <opencv2/opencv.hpp>

#include "box.h"
#include "libapi.h"

namespace yc
{
class LIB_API ConfParam
{
 public:
  ConfParam() : init_conf_(1), min_conf_(3), max_conf_(6) {}
  ConfParam(int init_conf, int min_conf, int max_conf)
      : init_conf_(init_conf), min_conf_(min_conf), max_conf_(max_conf)
  {
  }

 public:
  int init_conf_;
  int min_conf_;
  int max_conf_;
};

enum TRACK_STATUS
{
  MOVING,
  STATIONARY
};

class LIB_API Track
{
 public:
  Track(yc::ConfParam const& conf_param, MostProbDet const& det);
  Track(Track const& other);
  ~Track();

  Track& operator=(Track const& other);

  TRACK_STATUS GetStatus() const;

  int GetUniqueIndex() const;
  int GetCount() const;
  int GetConfidence() const;

  Box GetBox() const;
  int GetClassId() const;
  float GetClassProb() const;

  void Predict();
  void Correct(MostProbDet const& det);

 public:
  static void SetFps(double fps);

 private:
  class TrackImpl;
  TrackImpl* impl_;
};

class LIB_API TrackManager
{
 public:
  TrackManager(yc::ConfParam const& conf_param, double fps, double iou_thresh);
  TrackManager(TrackManager const& other);
  ~TrackManager();

  TrackManager& operator=(TrackManager const& other);

  void Clear();
  void Track(std::vector<MostProbDet> const& dets);

  void GetTracks(std::vector<yc::Track>& tracks);
  void GetSavedTracks(std::vector<yc::Track>& tracks);

 private:
  class TrackManagerImpl;
  TrackManagerImpl* impl_;
};
}  // namespace yc