module;
export module Utils;
import <vector>;
import <string>;
import <fstream>;

export namespace VulkanEngine {
std::vector<char> readFile(const std::string& filename);
}