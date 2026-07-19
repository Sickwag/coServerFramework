#pragma once

#include <memory>

namespace azzato {
namespace {

// Per-TU helper functions (internal linkage by design).
template <class T, class X, int N>
T& getInstanceX() {
	static T v;
	return v;
}

template <class T, class X, int N>
std::shared_ptr<T> getInstancePtr() {
	static std::shared_ptr<T> v(new T);
	return v;
}

}  // namespace

// The Singleton templates live in namespace azzato (NOT an anonymous namespace)
// so that all translation units share the same specialization and thus the same
// instance. Putting them in an anonymous namespace would give each TU its own copy.
template <class T, class X = void, int N = 0>
class Singleton {
  public:
	static T* getInstance() {
		static T v;
		return &v;
	}
};

template <class T, class X = void, int N = 0>
class SingletonPtr {
  public:
	static std::shared_ptr<T> getInstance() {
		static std::shared_ptr<T> v(new T);
		return v;
	}
};
}  // namespace azzato
