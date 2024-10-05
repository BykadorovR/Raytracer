#include "Graphic/Camera.h"

class CameraManager {
 private:
  std::map<std::string, std::shared_ptr<Camera>> _cameras;
  std::shared_ptr<Camera> _currentCamera;

 public:
  void addCamera(std::string name, std::shared_ptr<Camera> camera);
  std::shared_ptr<Camera> getCamera(std::string name);
  void setCurrentCamera(std::shared_ptr<Camera> camera);
  std::shared_ptr<Camera> getCurrentCamera();
};