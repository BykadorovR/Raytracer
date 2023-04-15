#include "Descriptor.h"

DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<Device> device) { _device = device; }

void DescriptorSetLayout::createCompute() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding imageLayoutBinding{};
  imageLayoutBinding.binding = 1;
  imageLayoutBinding.descriptorCount = 1;
  imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  imageLayoutBinding.pImmutableSamplers = nullptr;
  imageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uboLayoutBinding2{};
  uboLayoutBinding2.binding = 2;
  uboLayoutBinding2.descriptorCount = 1;
  uboLayoutBinding2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding2.pImmutableSamplers = nullptr;
  uboLayoutBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uboLayoutBinding3{};
  uboLayoutBinding3.binding = 3;
  uboLayoutBinding3.descriptorCount = 1;
  uboLayoutBinding3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding3.pImmutableSamplers = nullptr;
  uboLayoutBinding3.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uboLayoutBinding4{};
  uboLayoutBinding4.binding = 4;
  uboLayoutBinding4.descriptorCount = 1;
  uboLayoutBinding4.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding4.pImmutableSamplers = nullptr;
  uboLayoutBinding4.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding uboLayoutBinding5{};
  uboLayoutBinding5.binding = 5;
  uboLayoutBinding5.descriptorCount = 1;
  uboLayoutBinding5.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding5.pImmutableSamplers = nullptr;
  uboLayoutBinding5.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  std::array<VkDescriptorSetLayoutBinding, 6> bindings = {uboLayoutBinding,  imageLayoutBinding, uboLayoutBinding2,
                                                          uboLayoutBinding3, uboLayoutBinding4,  uboLayoutBinding5};
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void DescriptorSetLayout::createCamera() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

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

void DescriptorSetLayout::createGraphic() {
  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 0;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

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

void DescriptorSetLayout::createLight() {
  VkDescriptorSetLayoutBinding ssboLayoutBinding{};
  ssboLayoutBinding.binding = 0;
  ssboLayoutBinding.descriptorCount = 1;
  ssboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboLayoutBinding.pImmutableSamplers = nullptr;
  ssboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

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

  if (vkAllocateDescriptorSets(_device->getLogicalDevice(), &allocInfo, _descriptorSets.data()) != VK_SUCCESS) {
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

void DescriptorSet::createGraphic(std::shared_ptr<Texture> texture) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = texture->getImageView()->getImage()->getImageLayout();
    // TODO: make appropriate name
    imageInfo.imageView = texture->getImageView()->getImageView();
    imageInfo.sampler = texture->getSampler()->getSampler();

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = _descriptorSets[i];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), 1, &descriptorWrites, 0, nullptr);
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

void DescriptorSet::createCamera(std::shared_ptr<UniformBuffer> uniformBuffer) {
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

void DescriptorSet::createLight(std::shared_ptr<Buffer> buffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer->getSize();

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

void DescriptorSet::createJoints(std::shared_ptr<Buffer> buffer) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer->getSize();

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

void DescriptorSet::createCompute(std::vector<std::shared_ptr<Texture>> textureOut,
                                  std::shared_ptr<UniformBuffer> uniformBuffer,
                                  std::shared_ptr<UniformBuffer> uniformSpheres,
                                  std::shared_ptr<UniformBuffer> uniformRectangles,
                                  std::shared_ptr<UniformBuffer> uniformHitboxes,
                                  std::shared_ptr<UniformBuffer> uniformSettings) {
  for (size_t i = 0; i < _descriptorSets.size(); i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkDescriptorBufferInfo bufferInfo2{};
    bufferInfo2.buffer = uniformSpheres->getBuffer()[i]->getData();
    bufferInfo2.offset = 0;
    bufferInfo2.range = uniformSpheres->getBuffer()[i]->getSize();

    VkDescriptorBufferInfo bufferInfo3{};
    bufferInfo3.buffer = uniformRectangles->getBuffer()[i]->getData();
    bufferInfo3.offset = 0;
    bufferInfo3.range = uniformRectangles->getBuffer()[i]->getSize();

    VkDescriptorBufferInfo bufferInfo4{};
    bufferInfo4.buffer = uniformHitboxes->getBuffer()[i]->getData();
    bufferInfo4.offset = 0;
    bufferInfo4.range = uniformHitboxes->getBuffer()[i]->getSize();

    VkDescriptorBufferInfo bufferInfo5{};
    bufferInfo5.buffer = uniformSettings->getBuffer()[i]->getData();
    bufferInfo5.offset = 0;
    bufferInfo5.range = uniformSettings->getBuffer()[i]->getSize();

    VkDescriptorImageInfo imageInfoOut{};
    imageInfoOut.imageLayout = textureOut[i]->getImageView()->getImage()->getImageLayout();
    imageInfoOut.imageView = textureOut[i]->getImageView()->getImageView();

    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
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
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfoOut;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = _descriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &bufferInfo2;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = _descriptorSets[i];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &bufferInfo3;

    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = _descriptorSets[i];
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &bufferInfo4;

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = _descriptorSets[i];
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &bufferInfo5;

    vkUpdateDescriptorSets(_device->getLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

std::vector<VkDescriptorSet>& DescriptorSet::getDescriptorSets() { return _descriptorSets; }