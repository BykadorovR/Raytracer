#include "ComputePart.h"
#include "Input.h"
#include <random>

struct UniformCamera {
  float fov;
  alignas(16) glm::vec3 origin;
  alignas(16) glm::mat4 camera;
};

enum MaterialType { MATERIAL_DIFFUSE = 0, MATERIAL_METAL = 1, MATERIAL_DIELECTRIC = 2 };

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
  int index;
  UniformMaterial material;
};

struct UniformSpheres {
  int number;
  UniformSphere spheres[30];
};

struct HitBox {
  glm::vec3 center;
  glm::vec3 bias;
  int left;
  int right;
  int sphere;
};

struct UniformHitBox {
  int number = 0;
  HitBox hitbox[30];
};

HitBox mergeHitBoxes(UniformHitBox& hitBox, int left, int right) {
  auto leftHitBox = hitBox.hitbox[left];
  auto rightHitBox = hitBox.hitbox[right];
  HitBox result;
  float minX = std::min(leftHitBox.center.x - leftHitBox.bias.x, rightHitBox.center.x - rightHitBox.bias.x);
  float maxX = std::max(leftHitBox.center.x + leftHitBox.bias.x, rightHitBox.center.x + rightHitBox.bias.x);
  float minY = std::min(leftHitBox.center.y - leftHitBox.bias.y, rightHitBox.center.y - rightHitBox.bias.y);
  float maxY = std::max(leftHitBox.center.y + leftHitBox.bias.y, rightHitBox.center.y + rightHitBox.bias.y);
  float minZ = std::min(leftHitBox.center.z - leftHitBox.bias.z, rightHitBox.center.z - rightHitBox.bias.z);
  float maxZ = std::max(leftHitBox.center.z + leftHitBox.bias.z, rightHitBox.center.z + rightHitBox.bias.z);
  result.center = glm::vec3((minX + maxX) / 2.f, (minY + maxY) / 2.f, (minZ + maxZ) / 2.f);
  result.bias = glm::vec3(result.center.x - minX, result.center.y - minY, result.center.z - minZ);
  return result;
}

int calculateHitbox(std::vector<UniformSphere> spheres, UniformHitBox& hitBox) {
  HitBox current;
  if (spheres.size() == 1) {
    current.center = spheres[0].center;
    current.bias = glm::vec3(spheres[0].radius, spheres[0].radius, spheres[0].radius);
    current.left = -1;
    current.right = -1;
    current.sphere = spheres[0].index;
  } else {
    std::sort(spheres.begin(), spheres.end(), [](UniformSphere& left, UniformSphere& right) {
      return (left.center - left.radius).x < (right.center - right.radius).x;
    });

    int mid = spheres.size() / 2;
    auto left = calculateHitbox(std::vector<UniformSphere>(spheres.begin(), spheres.begin() + mid), hitBox);
    auto right = calculateHitbox(std::vector<UniformSphere>(spheres.begin() + mid, spheres.end()), hitBox);
    current = mergeHitBoxes(hitBox, left, right);
    current.left = left;
    current.right = right;
    current.sphere = -1;
  }
  int index = hitBox.number;
  hitBox.hitbox[index] = current;
  hitBox.number++;
  return index;
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
  _uniformBuffer2 = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformSpheres),
                                                    commandPool, queue, device);
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 1);
  std::uniform_real_distribution<> dist2(0, 0.5);
  std::uniform_real_distribution<> dist3(0.5, 1);

  UniformSpheres spheres;
  int current = 0;
  {
    UniformSphere sphere{};
    sphere.center = glm::vec3(0.f, -1000.f, 0.f);
    sphere.radius = 1000.f;
    sphere.index = current;
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.5, 0.5, 0.5);
    material.fuzz = 0;
    material.refraction = 1;
    sphere.material = material;
    spheres.spheres[0] = sphere;
  }

  /*
  for (int a = -11; a < 11; a++) {
    for (int b = -11; b < 11; b++) {
      float chooseMat = dist(e2);
      glm::vec3 center(a + 0.9 * dist(e2), 0.2, b + 0.9 * dist(e2));

      if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9) {
        if (chooseMat < 0.8) {
          // diffuse
          auto albedo = glm::vec3(dist(e2), dist(e2), dist(e2));
          {
            UniformSphere sphere{};
            sphere.center = center;
            sphere.radius = 0.2;
            UniformMaterial material{};
            material.type = MATERIAL_DIFFUSE;
            material.attenuation = albedo;
            material.fuzz = 0;
            material.refraction = 1;
            sphere.material = material;
            spheres.spheres[current++] = sphere;
          }
        } else if (chooseMat < 0.95) {
          // metal
          auto albedo = glm::vec3(dist3(e2), dist3(e2), dist3(e2));
          auto fuzz = dist2(e2);
          {
            UniformSphere sphere{};
            sphere.center = center;
            sphere.radius = 0.2;
            UniformMaterial material{};
            material.type = MATERIAL_METAL;
            material.attenuation = albedo;
            material.fuzz = fuzz;
            material.refraction = 1;
            sphere.material = material;
            spheres.spheres[current++] = sphere;
          }
        } else {
          // glass
          {
            UniformSphere sphere{};
            sphere.center = center;
            sphere.radius = 0.2;
            UniformMaterial material{};
            material.type = MATERIAL_DIELECTRIC;
            material.attenuation = glm::vec3(1.f, 1.f, 1.f);
            material.fuzz = 0;
            material.refraction = 1.f / 1.5f;
            sphere.material = material;
            spheres.spheres[current++] = sphere;
          }
        }
      }
    }
  }
  */
  {
    UniformSphere sphere{};
    sphere.center = glm::vec3(0, 1, 0);
    sphere.radius = 1.0;
    sphere.index = current;
    UniformMaterial material{};
    material.type = MATERIAL_DIELECTRIC;
    material.attenuation = glm::vec3(1.f, 1.f, 1.f);
    material.fuzz = 0;
    material.refraction = 1.f / 1.5f;
    sphere.material = material;
    spheres.spheres[current++] = sphere;
  }
  {
    UniformSphere sphere{};
    sphere.center = glm::vec3(-4, 1, 0);
    sphere.radius = 1.0;
    sphere.index = current;
    UniformMaterial material{};
    material.type = MATERIAL_DIFFUSE;
    material.attenuation = glm::vec3(0.4f, 0.2f, 0.1f);
    material.fuzz = 0;
    material.refraction = 1.f;
    sphere.material = material;
    spheres.spheres[current++] = sphere;
  }
  {
    UniformSphere sphere{};
    sphere.center = glm::vec3(4, 1, 0);
    sphere.radius = 1.0;
    sphere.index = current;
    UniformMaterial material{};
    material.type = MATERIAL_METAL;
    material.attenuation = glm::vec3(0.7f, 0.6f, 0.5f);
    material.fuzz = 0;
    material.refraction = 1.f;
    sphere.material = material;
    spheres.spheres[current++] = sphere;
  }
  spheres.number = current;

  UniformHitBox hitboxes;
  calculateHitbox(std::vector<UniformSphere>(spheres.spheres, spheres.spheres + spheres.number), hitboxes);

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBuffer2->getBuffer()[i]->getMemory(), 0, sizeof(spheres), 0,
                &data);
    memcpy(data, &spheres, sizeof(spheres));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer2->getBuffer()[i]->getMemory());
  }
  _descriptorSet->createCompute(_resultTextures, _uniformBuffer, _uniformBuffer2);
}

glm::vec3 from = glm::vec3(0, 2, 3);
glm::vec3 up = glm::vec3(0, 1, 0);
float cameraSpeed = 0.05f;
float deltaTime = 0.f;
float lastFrame = 0.f;
float fov = 90;
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

  UniformCamera ubo{};
  ubo.fov = glm::tan(glm::radians(fov) / 2.f);
  ubo.camera = glm::transpose(glm::lookAt(from, from + Input::direction, up));
  ubo.origin = from;

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
              &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                          0);
  vkCmdDispatch(_commandBuffer->getCommandBuffer()[currentFrame], std::get<0>(_settings->getResolution()) / 16,
                std::get<1>(_settings->getResolution()) / 16, 1);
}

std::vector<std::shared_ptr<Texture>> ComputePart::getResultTextures() { return _resultTextures; }

std::shared_ptr<Pipeline> ComputePart::getPipeline() { return _pipeline; }
std::shared_ptr<DescriptorSet> ComputePart::getDescriptorSet() { return _descriptorSet; }