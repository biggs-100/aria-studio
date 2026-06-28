#include "project.h"

namespace aria {

Project::Project() = default;
Project::~Project() { close(); }

bool Project::create(const std::string& path, const std::string& name) {
    path_ = path;
    name_ = name;
    modified_ = false;
    return true;
}

bool Project::open(const std::string& path) { path_ = path; return true; }
bool Project::save() { modified_ = false; return true; }
bool Project::save_as(const std::string& path) { path_ = path; return save(); }
void Project::close() { path_.clear(); name_.clear(); }

std::string Project::path() const { return path_; }
std::string Project::name() const { return name_; }
bool Project::is_modified() const { return modified_; }

} // namespace aria
