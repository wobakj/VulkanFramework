#ifndef APPLICATION_SINGLE_HPP
#define APPLICATION_SINGLE_HPP

namespace cmdline {
  class parser;
}

class Surface;
class SubmitInfo;
class FrameResource;

template<typename T>
class ApplicationSingle : public T {
 public:
  ApplicationSingle(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  virtual ~ApplicationSingle();
  
  void emptyDrawQueue() override;
  // default parser without arguments
  static cmdline::parser getParser();
  
 protected:
  virtual FrameResource createFrameResource() override;

  void shutDown();

  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const override;

 private:
  void render() override;

};

#include "app/application_single.inl"

#endif