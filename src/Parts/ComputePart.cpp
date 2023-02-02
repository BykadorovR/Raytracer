#include "ComputePart.h"
#include "Input.h"
#include <random>

glm::vec3 from = glm::vec3(0, 2, 3);
glm::vec3 up = glm::vec3(0, 1, 0);
float cameraSpeed = 0.05f;
float deltaTime = 0.f;
float lastFrame = 0.f;
float fov;

struct UniformCamera {
  float fov;
  alignas(16) glm::vec3 origin;
  alignas(16) glm::mat4 camera;
};

enum MaterialType { MATERIAL_DIFFUSE = 0, MATERIAL_METAL = 1, MATERIAL_DIELECTRIC = 2, MATERIAL_EMISSIVE = 3 };
enum PrimitiveType { PRIMITIVE_SPHERE = 0, PRIMITIVE_RECTANGLE = 1 };

struct UniformMaterial {
  int type;
  alignas(16) glm::vec3 attenuation;
  // actual only for metal
  float fuzz;
  // actual only for dielectric (etaIn / etaOut)
  float refraction;
};

struct UniformSphere {
  alignas(16) glm::vec3 center;
  float radius;
  UniformMaterial material;
};

struct UniformSpheres {
  int number;
  UniformSphere spheres[300];
};

struct UniformRectangle {
  alignas(16) glm::vec3 min;
  alignas(16) glm::vec3 max;
  int fixed;
  UniformMaterial material;
};

struct UniformRectangles {
  int number;
  UniformRectangle rectangles[300];
};

struct BoundingBox {
  glm::vec3 min;
  glm::vec3 max;
};

class Primitive {
 private:
  UniformMaterial _material;

 public:
  virtual BoundingBox getBB() = 0;
  void setMaterial(UniformMaterial material) { _material = material; }
  UniformMaterial getMaterial() { return _material; };
};

class Sphere : public Primitive {
 private:
  glm::vec3 _center;
  float _radius;

 public:
  Sphere(glm::vec3 center, float radius) : _center(center), _radius(radius) {}
  BoundingBox getBB() {
    glm::vec3 bias = glm::vec3(_radius, _radius, _radius);
    return BoundingBox{.min = _center - bias, .max = _center + bias};
  }
  glm::vec3 getCenter() { return _center; }
  float getRadius() { return _radius; }
};

class Rectangle : public Primitive {
 private:
  glm::vec3 _min;
  glm::vec3 _max;
  int _fixed;

 public:
  Rectangle(glm::vec3 min, glm::vec3 max, int fixed) : _min(min), _max(max), _fixed(fixed) {}
  BoundingBox getBB() {
    auto bb = BoundingBox{.min = glm::vec3(_min.x, _min.y, _min.z), .max = glm::vec3(_max.x, _max.y, _max.z)};
    bb.min[_fixed] -= 0.0001f;
    bb.max[_fixed] += 0.0001f;
    return bb;
  }
  glm::vec3 getMin() { return _min; }
  glm::vec3 getMax() { return _max; }
  int getFixed() { return _fixed; }
};

struct HitBoxTemp {
  BoundingBox bb;
  int index;
  int left;
  int right;
  int parent;
  std::shared_ptr<Primitive> primitive;
};

struct HitBox {
  alignas(16) glm::vec3 min;
  alignas(16) glm::vec3 max;
  int next;
  int exit;
  int primitive;
  int index;
};

struct UniformHitBox {
  int number;
  HitBox hitbox[300];
};

struct UniformSettings {
  int useBVH;
};

HitBoxTemp mergeHitBoxes(std::vector<HitBoxTemp>& hitBox, int left, int right) {
  auto leftHitBox = hitBox[left];
  auto rightHitBox = hitBox[right];
  HitBoxTemp result;
  glm::vec3 minLeft = leftHitBox.bb.min;
  glm::vec3 maxLeft = leftHitBox.bb.max;
  glm::vec3 minRight = rightHitBox.bb.min;
  glm::vec3 maxRight = rightHitBox.bb.max;

  glm::vec3 minBoth = glm::vec3(std::min(minLeft.x, minRight.x), std::min(minLeft.y, minRight.y),
                                std::min(minLeft.z, minRight.z));
  glm::vec3 maxBoth = glm::vec3(std::max(maxLeft.x, maxRight.x), std::max(maxLeft.y, maxRight.y),
                                std::max(maxLeft.z, maxRight.z));

  result.bb.min = minBoth;
  result.bb.max = maxBoth;
  return result;
}

void calculateThreadedBVH(std::vector<HitBoxTemp> hitBox,
                          UniformHitBox& hitBoxResult,
                          UniformSpheres& spheres,
                          UniformRectangles& rectangles) {
  auto addPrimitive = [&](int index) {
    if (std::dynamic_pointer_cast<Sphere>(hitBox[index].primitive)) {
      auto current = std::dynamic_pointer_cast<Sphere>(hitBox[index].primitive);
      spheres.spheres[spheres.number] = UniformSphere{.center = current->getCenter(),
                                                      .radius = current->getRadius(),
                                                      .material = current->getMaterial()};
      hitBoxResult.hitbox[index].primitive = PRIMITIVE_SPHERE;
      hitBoxResult.hitbox[index].index = spheres.number;
      spheres.number++;
    } else if (std::dynamic_pointer_cast<Rectangle>(hitBox[index].primitive)) {
      auto current = std::dynamic_pointer_cast<Rectangle>(hitBox[index].primitive);
      rectangles.rectangles[rectangles.number] = UniformRectangle{.min = current->getMin(),
                                                                  .max = current->getMax(),
                                                                  .fixed = current->getFixed(),
                                                                  .material = current->getMaterial()};
      hitBoxResult.hitbox[index].primitive = PRIMITIVE_RECTANGLE;
      hitBoxResult.hitbox[index].index = rectangles.number;
      rectangles.number++;
    } else {
      hitBoxResult.hitbox[index].primitive = -1;
      hitBoxResult.hitbox[index].index = -1;
    }
  };

  hitBoxResult.hitbox[0].next = -1;
  if (hitBox.size() > 1) hitBoxResult.hitbox[0].next = hitBox[1].index;
  hitBoxResult.hitbox[0].exit = -1;
  hitBoxResult.hitbox[0].min = hitBox[0].bb.min;
  hitBoxResult.hitbox[0].max = hitBox[0].bb.max;
  addPrimitive(0);

  for (int i = 1; i < hitBox.size() - 1; i++) {
    hitBoxResult.hitbox[i].next = hitBox[i + 1].index;
    hitBoxResult.hitbox[i].exit = -1;
    hitBoxResult.hitbox[i].min = hitBox[i].bb.min;
    hitBoxResult.hitbox[i].max = hitBox[i].bb.max;
    addPrimitive(i);

    if (hitBox[i].left == -1 && hitBox[i].right == -1) {
      hitBoxResult.hitbox[i].exit = hitBoxResult.hitbox[i].next;
    } else {
      auto parentNode = hitBox[hitBox[i].parent];
      if (parentNode.left == hitBox[i].index) {
        // left
        // internal
        hitBoxResult.hitbox[i].exit = parentNode.right;
      } else {
        // right
        // internal

        // check if parent exist and it's not right child
        while (parentNode.parent != -1) {
          auto grandparentNode = hitBox[parentNode.parent];
          if (grandparentNode.right != parentNode.index) {
            hitBoxResult.hitbox[i].exit = grandparentNode.right;
            break;
          }
          parentNode = grandparentNode;
        }
      }
    }
  }

  if (hitBox.size() > 1) {
    hitBoxResult.hitbox[hitBox.size() - 1].next = -1;
    hitBoxResult.hitbox[hitBox.size() - 1].exit = -1;
    hitBoxResult.hitbox[hitBox.size() - 1].min = hitBox[hitBox.size() - 1].bb.min;
    hitBoxResult.hitbox[hitBox.size() - 1].max = hitBox[hitBox.size() - 1].bb.max;
    addPrimitive(hitBox.size() - 1);
  }
}

int calculateHitbox(std::vector<std::shared_ptr<Primitive>> primitives, std::vector<HitBoxTemp>& hitBox, int parent) {
  HitBoxTemp current;
  int index = hitBox.size();
  hitBox.push_back(current);

  if (primitives.size() == 1) {
    auto bb = primitives[0]->getBB();
    hitBox[index].bb.min = bb.min;
    hitBox[index].bb.max = bb.max;
    hitBox[index].left = -1;
    hitBox[index].right = -1;
    hitBox[index].primitive = primitives[0];
  } else {
    int axis = rand() % 3;
    if (axis == 0) {
      std::sort(primitives.begin(), primitives.end(),
                [](std::shared_ptr<Primitive>& left, std::shared_ptr<Primitive>& right) {
                  return left->getBB().min.x < right->getBB().min.x;
                });
    } else if (axis == 1) {
      std::sort(primitives.begin(), primitives.end(),
                [](std::shared_ptr<Primitive>& left, std::shared_ptr<Primitive>& right) {
                  return left->getBB().min.y < right->getBB().min.y;
                });
    } else {
      std::sort(primitives.begin(), primitives.end(),
                [](std::shared_ptr<Primitive>& left, std::shared_ptr<Primitive>& right) {
                  return left->getBB().min.z < right->getBB().min.z;
                });
    }

    int mid = primitives.size() / 2;
    auto left = calculateHitbox(std::vector<std::shared_ptr<Primitive>>(primitives.begin(), primitives.begin() + mid),
                                hitBox, index);
    auto right = calculateHitbox(std::vector<std::shared_ptr<Primitive>>(primitives.begin() + mid, primitives.end()),
                                 hitBox, index);
    hitBox[index] = mergeHitBoxes(hitBox, left, right);
    hitBox[index].left = left;
    hitBox[index].right = right;
    hitBox[index].primitive = nullptr;
  }

  hitBox[index].index = index;
  hitBox[index].parent = parent;

  return index;
}

void sceneSpheres(std::vector<std::shared_ptr<Primitive>>& primitives) {
  fov = 90;
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 1);
  std::uniform_real_distribution<> dist2(0, 0.5);
  std::uniform_real_distribution<> dist3(0.5, 1);
  {
    std::shared_ptr<Sphere> sphere = std::make_shared<Sphere>(glm::vec3(0.f, -1000.f, 0.f), 1000.f);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.5, 0.5, 0.5);
    material.fuzz = 0;
    material.refraction = 1;
    sphere->setMaterial(material);
    primitives.push_back(sphere);
  }
  for (int a = -4; a < 4; a++) {
    for (int b = -4; b < 4; b++) {
      float chooseMat = dist(e2);
      glm::vec3 center(a + 0.9 * dist(e2), 0.2, b + 0.9 * dist(e2));
      if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9) {
        std::shared_ptr<Sphere> sphere = std::make_shared<Sphere>(center, 0.2f);
        primitives.push_back(sphere);
        if (chooseMat < 0.8) {
          // diffuse
          auto albedo = glm::vec3(dist(e2), dist(e2), dist(e2));
          {
            UniformMaterial material{};
            material.type = MATERIAL_DIFFUSE;
            material.attenuation = albedo;
            material.fuzz = 0;
            material.refraction = 1;
            sphere->setMaterial(material);
          }
        } else if (chooseMat < 0.95) {
          // metal
          auto albedo = glm::vec3(dist3(e2), dist3(e2), dist3(e2));
          auto fuzz = dist2(e2);
          {
            UniformMaterial material{};
            material.type = MATERIAL_METAL;
            material.attenuation = albedo;
            material.fuzz = fuzz;
            material.refraction = 1;
            sphere->setMaterial(material);
          }
        } else {
          // glass
          {
            UniformMaterial material{};
            material.type = MATERIAL_DIELECTRIC;
            material.attenuation = glm::vec3(1.f, 1.f, 1.f);
            material.fuzz = 0;
            material.refraction = 1.f / 1.5f;
            sphere->setMaterial(material);
          }
        }
      }
    }
  }
  {
    std::shared_ptr<Sphere> sphere = std::make_shared<Sphere>(glm::vec3(0, 1, 0), 1.0f);
    UniformMaterial material{};
    material.type = MATERIAL_DIELECTRIC;
    material.attenuation = glm::vec3(1.f, 1.f, 1.f);
    material.fuzz = 0;
    material.refraction = 1.f / 1.5f;
    sphere->setMaterial(material);
    primitives.push_back(sphere);
  }
  {
    std::shared_ptr<Sphere> sphere = std::make_shared<Sphere>(glm::vec3(-4, 1, 0), 1.0f);
    UniformMaterial material{};
    material.type = MATERIAL_EMISSIVE;
    material.attenuation = glm::vec3(0.4f, 0.2f, 0.1f);
    material.fuzz = 0;
    material.refraction = 1.f;
    sphere->setMaterial(material);
    primitives.push_back(sphere);
  }
  {
    std::shared_ptr<Sphere> sphere = std::make_shared<Sphere>(glm::vec3(4, 1, 0), 1.0f);
    UniformMaterial material{};
    material.type = MATERIAL_METAL;
    material.attenuation = glm::vec3(0.7f, 0.6f, 0.5f);
    material.fuzz = 0;
    material.refraction = 1.f;
    sphere->setMaterial(material);
    primitives.push_back(sphere);
  }
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1, 1, -3.f}, glm::vec3{1, 3, -3.f},
                                                                       2);
    UniformMaterial material{};
    material.type = MATERIAL_EMISSIVE;
    material.attenuation = glm::vec3(1.0f, 0.0f, 0.0f);
    material.fuzz = 0;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1, 4, -0.5}, glm::vec3{1, 4, 0.5}, 1);
    UniformMaterial material{};
    material.type = MATERIAL_EMISSIVE;
    material.attenuation = glm::vec3(1.0f, 1.0f, 1.0f);
    material.fuzz = 0;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
}

void sceneCornell(std::vector<std::shared_ptr<Primitive>>& primitives) {
  fov = 40;
  // right
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{1.f, 1, -1.}, glm::vec3{1.f, 3, 1.0},
                                                                       0);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.65f, 0.05f, 0.05f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // left
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1.f, 1, -1.}, glm::vec3{-1.f, 3, 1.0},
                                                                       0);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.12f, 0.45f, 0.15f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // middle
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1.f, 1, -1.}, glm::vec3{1.f, 3, -1.0},
                                                                       2);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.73f, 0.73f, 0.73f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // top
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1.f, 3, -1.}, glm::vec3{1.f, 3, 1.0},
                                                                       1);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.73f, 0.73f, 0.73f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // bottom
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-1.f, 1, -1.}, glm::vec3{1.f, 1, 1.0},
                                                                       1);
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.73f, 0.73f, 0.73f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // light
  {
    std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{-0.2f, 3, -0.4}, glm::vec3{0.2f, 3, 0},
                                                                       1);
    UniformMaterial material{};
    material.type = MATERIAL_EMISSIVE;
    material.attenuation = glm::vec3(15.f, 15.f, 15.f);
    material.fuzz = 0.f;
    material.refraction = 1.f;
    rectangle->setMaterial(material);
    primitives.push_back(rectangle);
  }
  // box tall
  {
    glm::vec3 min = {-0.6f, 1.f, -0.8f};
    glm::vec3 max = {0.f, 2.2f, -0.4f};
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(1.f, 0.73f, 0.73f);
    material.fuzz = 0.f;
    material.refraction = 1.f;

    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, max.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 2);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{max.x, max.y, min.z}, 2);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{max.x, min.y, max.z}, 1);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, max.y, min.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 1);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{min.x, max.y, max.z}, 0);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{max.x, min.y, min.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 0);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
  }

  // box short
  {
    glm::vec3 min = {-0.1f, 1.f, -0.3f};
    glm::vec3 max = {0.55f, 1.65f, 0.1f};
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(1.f, 0.73f, 0.73f);
    material.fuzz = 0.f;
    material.refraction = 1.f;

    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, max.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 2);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{max.x, max.y, min.z}, 2);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{max.x, min.y, max.z}, 1);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, max.y, min.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 1);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{min.x, min.y, min.z},
                                                                         glm::vec3{min.x, max.y, max.z}, 0);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
    {
      std::shared_ptr<Rectangle> rectangle = std::make_shared<Rectangle>(glm::vec3{max.x, min.y, min.z},
                                                                         glm::vec3{max.x, max.y, max.z}, 0);
      rectangle->setMaterial(material);
      primitives.push_back(rectangle);
    }
  }
}

ComputePart::ComputePart(std::shared_ptr<Device> device,
                         std::shared_ptr<Queue> queue,
                         std::shared_ptr<CommandBuffer> commandBuffer,
                         std::shared_ptr<CommandPool> commandPool,
                         std::shared_ptr<Settings> settings) {
  _device = device;
  _queue = queue;
  _commandBuffer = commandBuffer;
  _commandPool = commandPool;
  _settings = settings;

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // Image will be sampled in the fragment shader and used as storage target in the compute shader
    std::shared_ptr<Image> image = std::make_shared<Image>(
        settings->getResolution(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandPool, queue);
    std::shared_ptr<ImageView> imageView = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_COLOR_BIT, device);
    std::shared_ptr<Texture> texture = std::make_shared<Texture>(imageView, device);
    _resultTextures.push_back(texture);
  }

  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createCompute();

  _shader = std::make_shared<Shader>(device);
  _shader->add("../shaders/raytracing.spv", VK_SHADER_STAGE_COMPUTE_BIT);
  _pipeline = std::make_shared<Pipeline>(_shader, _descriptorSetLayout, device);
  _pipeline->createCompute();

  _descriptorPool = std::make_shared<DescriptorPool>(100, device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), _descriptorSetLayout,
                                                   _descriptorPool, device);
  _uniformBuffer = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformCamera), commandPool,
                                                   queue, device);
  _uniformBufferSpheres = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformSpheres),
                                                          commandPool, queue, device);
  _uniformBufferRectangles = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(),
                                                             sizeof(UniformRectangles), commandPool, queue, device);
  _uniformBufferHitboxes = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformHitBox),
                                                           commandPool, queue, device);
  _uniformBufferSettings = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformSettings),
                                                           commandPool, queue, device);

  std::vector<std::shared_ptr<Primitive>> primitives;
  sceneCornell(primitives);
  UniformHitBox hitboxes;
  std::vector<HitBoxTemp> hitboxTemp;
  calculateHitbox(primitives, hitboxTemp, -1);
  hitboxes.number = hitboxTemp.size();

  UniformSpheres spheres;
  spheres.number = 0;
  UniformRectangles rectangles;
  rectangles.number = 0;
  calculateThreadedBVH(hitboxTemp, hitboxes, spheres, rectangles);

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferSpheres->getBuffer()[i]->getMemory(), 0, sizeof(spheres), 0,
                &data);
    memcpy(data, &spheres, sizeof(spheres));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferSpheres->getBuffer()[i]->getMemory());
  }

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferRectangles->getBuffer()[i]->getMemory(), 0,
                sizeof(rectangles), 0, &data);
    memcpy(data, &rectangles, sizeof(rectangles));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferRectangles->getBuffer()[i]->getMemory());
  }

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferHitboxes->getBuffer()[i]->getMemory(), 0, sizeof(hitboxes),
                0, &data);
    memcpy(data, &hitboxes, sizeof(hitboxes));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferHitboxes->getBuffer()[i]->getMemory());
  }

  _descriptorSet->createCompute(_resultTextures, _uniformBuffer, _uniformBufferSpheres, _uniformBufferRectangles,
                                _uniformBufferHitboxes, _uniformBufferSettings);

  _checkboxes["use_bvh"] = new bool();
}

std::map<std::string, bool*> ComputePart::getCheckboxes() { return _checkboxes; }

void ComputePart::draw(int currentFrame) {
  float currentTime = static_cast<float>(glfwGetTime());
  deltaTime = currentTime - lastFrame;
  lastFrame = currentTime;

  if (Input::keyW) from += deltaTime * Input::direction;
  if (Input::keyS) from -= deltaTime * Input::direction;
  if (Input::keyA) from -= glm::normalize(glm::cross(Input::direction, up)) * deltaTime;
  if (Input::keyD) from += glm::normalize(glm::cross(Input::direction, up)) * deltaTime;
  if (Input::keySpace) {
    from += deltaTime * up;
  }

  {
    void* data;
    UniformSettings uboSettings{};
    uboSettings.useBVH = *(_checkboxes["use_bvh"]);
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferSettings->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(uboSettings), 0, &data);
    memcpy(data, &uboSettings, sizeof(uboSettings));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferSettings->getBuffer()[currentFrame]->getMemory());
  }

  {
    UniformCamera ubo{};
    ubo.fov = glm::tan(glm::radians(fov) / 2.f);
    ubo.camera = glm::transpose(glm::lookAt(from, from + Input::direction, up));
    ubo.origin = from;
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
                &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());
  }

  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                          0);
  vkCmdDispatch(_commandBuffer->getCommandBuffer()[currentFrame], std::get<0>(_settings->getResolution()) / 8,
                std::get<1>(_settings->getResolution()) / 8, 1);
}

std::vector<std::shared_ptr<Texture>> ComputePart::getResultTextures() { return _resultTextures; }

std::shared_ptr<Pipeline> ComputePart::getPipeline() { return _pipeline; }
std::shared_ptr<DescriptorSet> ComputePart::getDescriptorSet() { return _descriptorSet; }