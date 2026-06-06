#pragma once

namespace azzato {
class NoCopyable {
  public:
	NoCopyable()								   = default;
	~NoCopyable()								   = default;
	NoCopyable(const NoCopyable& other)			   = delete;
	NoCopyable& operator=(NoCopyable const& other) = delete;
};
}  // namespace azzato
