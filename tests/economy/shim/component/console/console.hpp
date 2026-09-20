#pragma once
namespace console {
template<class... T> void error(const char*, T...) {}
template<class... T> void warn(const char*, T...) {}
template<class... T> void info(const char*, T...) {}
template<class... T> void debug(const char*, T...) {}
}
