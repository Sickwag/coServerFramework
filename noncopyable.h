#pragma once

namespace azzato {
class Noncopyable {
  public:
	Noncopyable()									 = default;
	~Noncopyable()									 = default;
	Noncopyable(Noncopyable const& other)			 = delete;
	Noncopyable& operator=(Noncopyable const& other) = delete;
};
}  // namespace azzato
