#include "Camera.h"
#include "Primitive/Drawable.h"
#undef near
#undef far

class CameraRTS : public Camera, public InputSubscriber {
 private:
  std::shared_ptr<Drawable> _object;
  std::shared_ptr<EngineState> _engineState;
  float _fov;
  float _aspect;
  glm::vec3 _shift = glm::vec3(0.f, 10.f, 0.f);
  int _threshold = 100;
  float _moveSpeed = 5.f;
  bool _attached = true;

 public:
  CameraRTS(std::shared_ptr<Drawable> object, std::shared_ptr<EngineState> engineState);
  void setProjectionParameters(float fov, float near, float far);
  void setAspect(float aspect);
  void setShift(glm::vec3 shift);
  void setThreshold(int threshold);

  glm::mat4 getView() override;
  glm::mat4 getProjection() override;

  void cursorNotify(float xPos, float yPos) override;
  void mouseNotify(int button, int action, int mods) override;
  void keyNotify(int key, int scancode, int action, int mods) override;
  void charNotify(unsigned int code) override;
  void scrollNotify(double xOffset, double yOffset) override;
};