#include "track_manager.h"

#include <deque>

#include "hungarian/Hungarian.h"
#include "utils.h"

#define SQUARE(x) ((x) * (x))

namespace yc
{
class Track::TrackImpl
{
 public:
  TrackImpl();
  TrackImpl(yc::ConfParam const& conf_param, MostProbDet const& det);

  static void SetFps(double fps);

  TRACK_STATUS GetStatus() const;

  int GetUniqueIndex() const;
  int GetCount() const;
  int GetConfidence() const;

  Box GetBox() const;
  int GetClassId() const;
  float GetClassProb() const;

  void Predict();
  void Correct(MostProbDet const& det);
  void InitKalmanFilter(cv::Point2f const& point);

 public:
  static double fps_;
  static int shared_counter_;

  TRACK_STATUS status_;
  std::deque<Box> pt_history_;

  cv::KalmanFilter kf_;

  int unique_idx_;
  int count_;

  yc::ConfParam conf_param_;
  int conf_;

  MostProbDet det_;
};

double Track::TrackImpl::fps_ = 0;
int Track::TrackImpl::shared_counter_ = 0;

Track::TrackImpl::TrackImpl() {}
Track::TrackImpl::TrackImpl(
    yc::ConfParam const& conf_param, MostProbDet const& det)
    : status_(MOVING),
      unique_idx_(shared_counter_++),
      count_(1),
      conf_param_(conf_param),
      conf_(conf_param.init_conf_),
      det_(det)
{
  InitKalmanFilter(cv::Point2f(det.bbox.x, det.bbox.y));
}

TRACK_STATUS Track::TrackImpl::GetStatus() const { return status_; }

int Track::TrackImpl::GetUniqueIndex() const { return unique_idx_; }
int Track::TrackImpl::GetCount() const { return count_; }
int Track::TrackImpl::GetConfidence() const { return conf_; }

Box Track::TrackImpl::GetBox() const { return det_.bbox; }
int Track::TrackImpl::GetClassId() const { return det_.cid; }
float Track::TrackImpl::GetClassProb() const { return det_.prob; }

void Track::TrackImpl::Predict()
{
  if (status_ == MOVING)
  {
    cv::Mat result = kf_.predict();
    det_.bbox.x = result.at<float>(0);
    det_.bbox.y = result.at<float>(1);
    conf_--;
  }

  count_++;
}

void Track::TrackImpl::Correct(MostProbDet const& det)
{
  Box const& bbox = det.bbox;

  if (status_ == MOVING)
  {
    cv::Mat result = kf_.correct((cv::Mat_<float>(2, 1) << bbox.x, bbox.y));

    det_.bbox.x = result.at<float>(0);
    det_.bbox.y = result.at<float>(1);
    det_.bbox.w = (det_.bbox.w + bbox.w) / 2;
    det_.bbox.h = (det_.bbox.h + bbox.h) / 2;
    det_.prob = (det_.prob + det.prob) / 2;

    conf_ = min_val_cmp(conf_param_.max_conf_, conf_ + 2);
  }
  else
  {
    det_.bbox.x = 0.9 * det_.bbox.x + 0.1 * bbox.x;
    det_.bbox.y = 0.9 * det_.bbox.y + 0.1 * bbox.y;
    det_.bbox.w = 0.9 * det_.bbox.w + 0.1 * bbox.w;
    det_.bbox.h = 0.9 * det_.bbox.h + 0.1 * bbox.h;
  }

  // status change
  pt_history_.push_back(det_.bbox);
  if (pt_history_.size() > fps_ * 10)
    pt_history_.pop_front();

  if (pt_history_.size() < fps_)
    return;

  Box bbox1 = pt_history_.front();
  Box bbox2 = pt_history_.back();
  if (Box::Iou(bbox1, bbox2) > 0.7)
    status_ = STATIONARY;
  else
    status_ = MOVING;
}

void Track::TrackImpl::InitKalmanFilter(cv::Point2f const& point)
{
  kf_.init(4, 2);
  kf_.transitionMatrix =
      (cv::Mat_<float>(4, 4) << 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1);

  setIdentity(kf_.measurementMatrix);
  setIdentity(kf_.processNoiseCov, cv::Scalar::all(1e-4f));
  setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(2e-4f));
  kf_.statePost = (cv::Mat_<float>(4, 1) << point.x, point.y, 0.0f, 0.0f);
}
///

///
Track::Track(yc::ConfParam const& conf_param, MostProbDet const& det)
    : impl_(new TrackImpl(conf_param, det))
{
}

Track::Track(Track const& other) : impl_(new TrackImpl)
{
  impl_->status_ = other.impl_->status_;
  impl_->pt_history_ = other.impl_->pt_history_;

  impl_->kf_ = other.impl_->kf_;

  impl_->unique_idx_ = other.impl_->unique_idx_;

  impl_->conf_param_ = other.impl_->conf_param_;
  impl_->conf_ = other.impl_->conf_;

  impl_->det_ = other.impl_->det_;
}

Track::~Track() { delete impl_; }

Track& Track::operator=(Track const& other)
{
  if (this != &other)
  {
    impl_->status_ = other.impl_->status_;
    impl_->pt_history_ = other.impl_->pt_history_;

    impl_->kf_ = other.impl_->kf_;

    impl_->unique_idx_ = other.impl_->unique_idx_;

    impl_->conf_param_ = other.impl_->conf_param_;
    impl_->conf_ = other.impl_->conf_;

    impl_->det_ = other.impl_->det_;
  }

  return *this;
}

TRACK_STATUS Track::GetStatus() const { return impl_->GetStatus(); }

int Track::GetUniqueIndex() const { return impl_->GetUniqueIndex(); }
int Track::GetCount() const { return impl_->GetCount(); }
int Track::GetConfidence() const { return impl_->GetConfidence(); }

Box Track::GetBox() const { return impl_->GetBox(); }
int Track::GetClassId() const { return impl_->GetClassId(); }
float Track::GetClassProb() const { return impl_->GetClassProb(); }

void Track::Predict() { impl_->Predict(); }
void Track::Correct(MostProbDet const& det) { impl_->Correct(det); }

void Track::SetFps(double fps) { TrackImpl::fps_ = fps; }
///

///
class TrackManager::TrackManagerImpl
{
 public:
  TrackManagerImpl();
  TrackManagerImpl(
      yc::ConfParam const& conf_param, double fps, double dist_thresh);

  void Clear();
  void Track(std::vector<MostProbDet> const& dets);

  void GetTracks(std::vector<yc::Track>& tracks);
  void GetSavedTracks(std::vector<yc::Track>& tracks);

  cv::Mat Associate(std::vector<MostProbDet> const& dets);
  bool ConstructSimMat(Matrix& sim_mat, std::vector<MostProbDet> const& dets);

 public:
  yc::ConfParam conf_param_;
  double iou_thresh_;

  std::vector<yc::Track> tracks_;
  std::vector<yc::Track> saved_tracks_;
};

TrackManager::TrackManagerImpl::TrackManagerImpl() {}

TrackManager::TrackManagerImpl::TrackManagerImpl(
    yc::ConfParam const& conf_param, double fps, double dist_thresh)
    : conf_param_(conf_param), iou_thresh_(dist_thresh)
{
  Track::SetFps(fps);
}

void TrackManager::TrackManagerImpl::Clear()
{
  tracks_.clear();
  tracks_.shrink_to_fit();
}

void TrackManager::TrackManagerImpl::Track(std::vector<MostProbDet> const& dets)
{
  if (tracks_.size() != 0)
  {
    // predict existing tracks
    for (size_t i = 0; i < tracks_.size(); i++)
    {
      tracks_[i].Predict();
    }

    if (dets.size() != 0)
    {
      cv::Mat table = Associate(dets);

      // correct existing tracks
      for (int i = 0; i < (int)tracks_.size(); i++)
      {
        for (int j = 0; j < (int)dets.size(); j++)
        {
          if (!table.at<int>(i, j))
            continue;

          tracks_[i].Correct(dets[j]);
        }
      }

      table = table.t();  // transpose matrix to improve performance
      int num_tracks = (int)tracks_.size();

      // launch new tracks
      for (int i = 0; i < (int)dets.size(); i++)
      {
        int sum = 0;
        for (int j = 0; j < num_tracks; j++)
        {
          sum += table.at<int>(i, j);
        }

        if (!sum)
          tracks_.push_back(yc::Track(conf_param_, dets[i]));
      }
    }
  }
  else
  {
    // launch new tracks
    for (int i = 0; i < (int)dets.size(); i++)
    {
      tracks_.push_back(yc::Track(conf_param_, dets[i]));
    }
  }

  // delete tracks
  std::vector<yc::Track> remaining_tracks;

  for (size_t i = 0; i < tracks_.size(); i++)
  {
    if (tracks_[i].GetConfidence() > 0)
    {
      remaining_tracks.push_back(tracks_[i]);
    }
    else
    {
      if (tracks_[i].GetCount() > 30)
        saved_tracks_.push_back(tracks_[i]);
    }
  }

  tracks_ = remaining_tracks;
}

void TrackManager::TrackManagerImpl::GetTracks(std::vector<yc::Track>& tracks)
{
  tracks.clear();
  for (size_t i = 0; i < tracks_.size(); i++)
  {
    if (tracks_[i].GetConfidence() >= conf_param_.min_conf_)
      tracks.push_back(tracks_[i]);
  }
}

void TrackManager::TrackManagerImpl::GetSavedTracks(
    std::vector<yc::Track>& tracks)
{
  tracks = saved_tracks_;
}

cv::Mat TrackManager::TrackManagerImpl::Associate(
    std::vector<MostProbDet> const& dets)
{
  Matrix sim_mat;
  bool is_trans = ConstructSimMat(sim_mat, dets);

  BipartiteGraph bg(sim_mat);
  Hungarian kuhn_munkres(bg);
  kuhn_munkres.HungarianAlgo();

  cv::Mat match;
  BipartiteGraph* bg_result = kuhn_munkres.GetBG();
  if (is_trans)
    match = cv::Mat::zeros((int)bg_result->GetNumTasks(),
        (int)bg_result->GetNumAgents(), CV_32SC1);
  else
    match = cv::Mat::zeros((int)bg_result->GetNumAgents(),
        (int)bg_result->GetNumTasks(), CV_32SC1);

  for (int i = 0; i < match.rows; i++)
  {
    for (int j = 0; j < match.cols; j++)
    {
      Edge* edge = nullptr;
      if (is_trans)
        edge = bg_result->GetMatrix(j, i);
      else
        edge = bg_result->GetMatrix(i, j);

      if (edge->GetMatchedFlag() && edge->GetWeight() > iou_thresh_)
        match.at<int>(i, j) = 1;
    }
  }

  return match;
}

bool TrackManager::TrackManagerImpl::ConstructSimMat(
    Matrix& sim_mat, std::vector<MostProbDet> const& dets)
{
  bool is_trans = false;
  if (tracks_.size() > dets.size())
    is_trans = true;

  std::vector<Box> agents;

  if (is_trans)
  {
    for (size_t i = 0; i < dets.size(); i++)
    {
      agents.push_back(dets[i].bbox);
    }
  }
  else
  {
    for (size_t i = 0; i < tracks_.size(); i++)
    {
      agents.push_back(tracks_[i].GetBox());
    }
  }

  std::vector<Box> tasks;

  if (is_trans)
  {
    for (size_t i = 0; i < tracks_.size(); i++)
    {
      tasks.push_back(tracks_[i].GetBox());
    }
  }
  else
  {
    for (size_t i = 0; i < dets.size(); i++)
    {
      tasks.push_back(dets[i].bbox);
    }
  }

  // Allocate matrix
  sim_mat.resize(agents.size());
  for (size_t i = 0; i < sim_mat.size(); i++)
  {
    sim_mat[i].resize(tasks.size());
  }

  for (size_t i = 0; i < agents.size(); i++)
  {
    for (size_t j = 0; j < tasks.size(); j++)
    {
      sim_mat[i][j].SetWeight(Box::Iou(agents[i], tasks[j]));
    }
  }

  return is_trans;
};

TrackManager::TrackManager(
    yc::ConfParam const& conf_param, double fps, double dist_thresh)
    : impl_(new TrackManagerImpl(conf_param, fps, dist_thresh))
{
}

TrackManager::TrackManager(TrackManager const& other)
    : impl_(new TrackManagerImpl)
{
  impl_->conf_param_ = other.impl_->conf_param_;
  impl_->iou_thresh_ = other.impl_->iou_thresh_;

  impl_->tracks_ = other.impl_->tracks_;
  impl_->saved_tracks_ = other.impl_->saved_tracks_;
}

TrackManager::~TrackManager() { delete impl_; }

TrackManager& TrackManager::operator=(TrackManager const& other)
{
  if (this != &other)
  {
    impl_->conf_param_ = other.impl_->conf_param_;
    impl_->iou_thresh_ = other.impl_->iou_thresh_;

    impl_->tracks_ = other.impl_->tracks_;
    impl_->saved_tracks_ = other.impl_->saved_tracks_;
  }

  return *this;
}

void TrackManager::Clear() { impl_->Clear(); }
void TrackManager::Track(std::vector<MostProbDet> const& dets)
{
  impl_->Track(dets);
}

void TrackManager::GetTracks(std::vector<yc::Track>& tracks)
{
  impl_->GetTracks(tracks);
}
void TrackManager::GetSavedTracks(std::vector<yc::Track>& tracks)
{
  impl_->GetSavedTracks(tracks);
}
}  // namespace yc