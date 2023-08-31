#include "Descriptor.h"

DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<Device> device) { _device = device; }

void DescriptorSetLayout::createPostprocessing() {
  std::array<VkDescriptorSetLayoutBinding, 2> bindings;
  bindings[0].binding = 0;
  bindings[0].descriptorCount = 1;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  bindings[0].pImmutableSamplers = nullptr;
  bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorCount = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  bindings[1].pImmutableSamplers = nullptr;
  bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = bindings.size();
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createParticleComputeBuffer() {
  std::array<VkDescriptorSetLayoutBinding, 3> layoutBinding{};
  layoutBinding[0].binding = 0;
  layoutBinding[0].descriptorCount = 1;
  layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layoutBinding[0].pImmutableSamplers = nullptr;
  layoutBinding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  layoutBinding[1].binding = 1;
  layoutBinding[1].descriptorCount = 1;
  layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBinding[1].pImmutableSamplers = nullptr;
  layoutBinding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  layoutBinding[2].binding = 2;
  layoutBinding[2].descriptorCount = 1;
  layoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBinding[2].pImmutableSamplers = nullptr;
  layoutBinding[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = layoutBinding.size();
  layoutInfo.pBindings = layoutBinding.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createUniformBuffer(VkShaderStageFlags stage) {
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = 0;
  layoutBinding.descriptorCount = 1;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layoutBinding.pImmutableSamplers = nullptr;
  layoutBinding.stageFlags = stage;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &layoutBinding;

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createGraphicModel() {
  std::array<VkDescriptorSetLayoutBinding, 2> bindings;
  bindings[0].binding = 0;
  bindings[0].descriptorCount = 1;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[0].pImmutableSamplers = nullptr;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorCount = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].pImmutableSamplers = nullptr;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = bindings.size();
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createTexture(int number, int binding, VkShaderStageFlags stage) {
  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = binding;
  samplerLayoutBinding.descriptorCount = number;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = stage;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerLayoutBinding;

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createModelAuxilary() {
  VkDescriptorSetLayoutBinding modelAuxilary{};
  modelAuxilary.binding = 0;
  modelAuxilary.descriptorCount = 1;
  modelAuxilary.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  modelAuxilary.pImmutableSamplers = nullptr;
  modelAuxilary.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &modelAuxilary;

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createShadowTexture() {
  std::array<VkDescriptorSetLayoutBinding, 2> bindings;
  bindings[0].binding = 0;
  bindings[0].descriptorCount = 2;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[0].pImmutableSamplers = nullptr;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorCount = 4;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].pImmutableSamplers = nullptr;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = bindings.size();
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createLightVP(VkShaderStageFlagBits stage) {
  std::array<VkDescriptorSetLayoutBinding, 2> ssboLayoutBinding{};
  ssboLayoutBinding[0].binding = 0;
  ssboLayoutBinding[0].descriptorCount = 1;
  ssboLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding[0].pImmutableSamplers = nullptr;
  ssboLayoutBinding[0].stageFlags = stage;

  ssboLayoutBinding[1].binding = 1;
  ssboLayoutBinding[1].descriptorCount = 1;
  ssboLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding[1].pImmutableSamplers = nullptr;
  ssboLayoutBinding[1].stageFlags = stage;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = ssboLayoutBinding.size();
  layoutInfo.pBindings = ssboLayoutBinding.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createLight() {
  std::array<VkDescriptorSetLayoutBinding, 2> ssboLayoutBinding{};
  ssboLayoutBinding[0].binding = 0;
  ssboLayoutBinding[0].descriptorCount = 1;
  ssboLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding[0].pImmutableSamplers = nullptr;
  ssboLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  ssboLayoutBinding[1].binding = 1;
  ssboLayoutBinding[1].descriptorCount = 1;
  ssboLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding[1].pImmutableSamplers = nullptr;
  ssboLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = ssboLayoutBinding.size();
  layoutInfo.pBindings = ssboLayoutBinding.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createJoints() {
  VkDescriptorSetLayoutBinding ssboLayoutBinding{};
  ssboLayoutBinding.binding = 0;
  ssboLayoutBinding.descriptorCount = 1;
  ssboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding.pImmutableSamplers = nullptr;
  ssboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding bindings = ssboLayoutBinding;
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &bindings;

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createGUI() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

VkDescriptorSetLayout& DescriptorSetLayout::getDescriptorSetLayout() { return _descriptorSetLayout; }

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptorSetLayout, nullptr);
}

DescriptorPool::DescriptorPool(int number, std::shared_ptr<Device> device) {
  _device = device;

  std::array<VkDescriptorPoolSize, 4> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(number);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(number);
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  poolSizes[2].descriptorCount = static_cast<uint32_t>(number);
  poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[3].descriptorCount = static_cast<uint32_t>(number);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(number);

  if (vkCreateDescriptorPool(device->getLogicalDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

VkDescriptorPool& DescriptorPool::getDescriptorPool() { return _descriptorPool; }

DescriptorPool::~DescriptorPool() { vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptorPool, nullptr); }

DescriptorSet::DescriptorSet(int number,
                             std::shared_ptr<DescriptorSetLayout> layout,
                             std::shared_ptr<DescriptorPool> pool,
                             std::shared_ptr<Device> device) {
  _descriptorSets.resize(number);
  _device = device;

  std::vector<VkDescriptorSetLayout> layouts(_descriptorSets.size(), layout->getDescriptorSetLayout());
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool->getDescriptorPool();
  allocInfo.descriptorSetCount = static_cast<uint32_t>(_descriptorSets.size());
  allocInfo.pSetLayouts = layouts.data();

  auto sts = vkAllocateDescriptorSets(_device->getLogicalDevice(), &allocInfo, _descriptorSets.data());
  if (sts != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }
}

void DescriptorSet::createGraphicModel(std::shared_ptr<Texture> texture, std::shared_ptr<Texture> normal) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = texture->getImageView()->getImage()->getImageLayout();
    // TODO: make appropriate name
    imageInfo.imageView = texture->getImageView()->getImageView();
    imageInfo.sampler = texture->getSampler()->getSampler();

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = normal->getImageView()->getImage()->getImageLayout();
    // TODO: make appropriate name
    normalInfo.imageView = normal->getImageView()->getImageView();
    normalInfo.sampler = normal->getSampler()->getSampler();

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &normalInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

void DescriptorSet::createPostprocessing(std::shared_ptr<ImageView> src, std::shared_ptr<ImageView> dst) {
  VkDescriptorImageInfo imageInfoSrc{};
  imageInfoSrc.imageLayout = src->getImage()->getImageLayout();
  // TODO: make appropriate name
  imageInfoSrc.imageView = src->getImageView();

  VkDescriptorImageInfo imageInfoDst{};
  imageInfoDst.imageLayout = dst->getImage()->getImageLayout();
  // TODO: make appropriate name
  imageInfoDst.imageView = dst->getImageView();

  std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = _descriptorSets[0];
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &imageInfoSrc;

  descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[1].dstSet = _descriptorSets[0];
  descriptorWrites[1].dstBinding = 1;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pImageInfo = &imageInfoDst;

  vkUpdateDescriptorSets(_device->getLogicalDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void DescriptorSet::createTexture(std::vector<std::shared_ptr<Texture>> texture, int binding) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    std::vector<VkDescriptorImageInfo> imageInfo(texture.size());
    for (int j = 0; j < texture.size(); j++) {
      imageInfo[j].imageLayout = texture[j]->getImageView()->getImage()->getImageLayout();
      // TODO: make appropriate name
      imageInfo[j].imageView = texture[j]->getImageView()->getImageView();
      imageInfo[j].sampler = texture[j]->getSampler()->getSampler();
    }

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = _descriptorSets[i];
    descriptorWrites.dstBinding = binding;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites.descriptorCount = imageInfo.size();
    descriptorWrites.pImageInfo = imageInfo.data();

    vkUpdateDescriptorSets(_device->getLogicalDevice(), 1, &descriptorWrites, 0, nullptr);
  }
}

void DescriptorSet::createShadowTexture(std::vector<std::shared_ptr<Texture>> directional,
                                        std::vector<std::shared_ptr<Texture>> point) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    std::vector<VkDescriptorImageInfo> directionalImageInfo(directional.size());
    for (int j = 0; j < directionalImageInfo.size(); j++) {
      directionalImageInfo[j].imageLayout = directional[j]->getImageView()->getImage()->getImageLayout();
      // TODO: make appropriate name
      directionalImageInfo[j].imageView = directional[j]->getImageView()->getImageView();
      directionalImageInfo[j].sampler = directional[j]->getSampler()->getSampler();
    }

    std::vector<VkDescriptorImageInfo> pointImageInfo(point.size());
    for (int j = 0; j < pointImageInfo.size(); j++) {
      pointImageInfo[j].imageLayout = point[j]->getImageView()->getImage()->getImageLayout();
      // TODO: make appropriate name
      pointImageInfo[j].imageView = point[j]->getImageView()->getImageView();
      pointImageInfo[j].sampler = point[j]->getSampler()->getSampler();
    }

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = directionalImageInfo.size();
    descriptorWrites[0].pImageInfo = directionalImageInfo.data();

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = pointImageInfo.size();
    descriptorWrites[1].pImageInfo = pointImageInfo.data();

    vkUpdateDescriptorSets(_device->getLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

void DescriptorSet::createModelAuxilary(std::shared_ptr<UniformBuffer> uniformBuffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = _descriptorSets[i];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), 1, &descriptorWrites, 0, nullptr);
  }
}

void DescriptorSet::createParticleComputeBuffer(std::shared_ptr<UniformBuffer> uniformBuffer,
                                                std::shared_ptr<Buffer> bufferIn,
                                                std::shared_ptr<Buffer> bufferOut) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferUniformInfo{};
    bufferUniformInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferUniformInfo.offset = 0;
    bufferUniformInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkDescriptorBufferInfo bufferInInfo{};
    bufferInInfo.buffer = bufferIn->getData();
    bufferInInfo.offset = 0;
    bufferInInfo.range = bufferIn->getSize();

    VkDescriptorBufferInfo bufferOutInfo{};
    bufferOutInfo.buffer = bufferOut->getData();
    bufferOutInfo.offset = 0;
    bufferOutInfo.range = bufferOut->getSize();

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferUniformInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = _descriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &bufferOutInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
  }
}

void DescriptorSet::createUniformBuffer(std::shared_ptr<UniformBuffer> uniformBuffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = _descriptorSets[i];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), 1, &descriptorWrites, 0, nullptr);
  }
}

void DescriptorSet::createLight(int currentFrame,
                                std::vector<std::shared_ptr<Buffer>> bufferDirectional,
                                std::vector<std::shared_ptr<Buffer>> bufferPoint) {
  VkDescriptorBufferInfo bufferDirectionalInfo{};
  bufferDirectionalInfo.buffer = VK_NULL_HANDLE;
  bufferDirectionalInfo.offset = 0;
  bufferDirectionalInfo.range = VK_WHOLE_SIZE;
  if (bufferDirectional.size() > currentFrame && bufferDirectional[currentFrame]) {
    bufferDirectionalInfo.buffer = bufferDirectional[currentFrame]->getData();
    bufferDirectionalInfo.range = bufferDirectional[currentFrame]->getSize();
  }

  VkDescriptorBufferInfo bufferPointInfo{};
  bufferPointInfo.buffer = VK_NULL_HANDLE;
  bufferPointInfo.offset = 0;
  bufferPointInfo.range = VK_WHOLE_SIZE;
  if (bufferPoint.size() > currentFrame && bufferPoint[currentFrame]) {
    bufferPointInfo.buffer = bufferPoint[currentFrame]->getData();
    bufferPointInfo.range = bufferPoint[currentFrame]->getSize();
  }
  std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = _descriptorSets[currentFrame];
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pBufferInfo = &bufferDirectionalInfo;

  descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[1].dstSet = _descriptorSets[currentFrame];
  descriptorWrites[1].dstBinding = 1;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pBufferInfo = &bufferPointInfo;

  vkUpdateDescriptorSets(_device->getLogicalDevice(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void DescriptorSet::createJoints(std::vector<std::shared_ptr<Buffer>> buffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer[i]->getSize();

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = _descriptorSets[i];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), 1, &descriptorWrites, 0, nullptr);
  }
}

void DescriptorSet::createGUI(std::shared_ptr<Texture> texture, std::shared_ptr<UniformBuffer> uniformBuffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = texture->getImageView()->getImage()->getImageLayout();
    // TODO: make appropriate name
    imageInfo.imageView = texture->getImageView()->getImageView();
    imageInfo.sampler = texture->getSampler()->getSampler();

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

std::vector<VkDescriptorSet>& DescriptorSet::getDescriptorSets() { return _descriptorSets; }