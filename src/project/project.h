#ifndef ARIA_PROJECT_H
#define ARIA_PROJECT_H

#include <cstdint>
#include <string>

namespace aria {

/// Project — project document model with serialization.
class Project {
public:
    Project();
    ~Project();

    bool create(const std::string& path, const std::string& name);
    bool open(const std::string& path);
    bool save();
    bool save_as(const std::string& path);
    void close();

    std::string path() const;
    std::string name() const;
    bool is_modified() const;

private:
    std::string path_;
    std::string name_;
    bool modified_ = false;
};

} // namespace aria

#endif // ARIA_PROJECT_H
