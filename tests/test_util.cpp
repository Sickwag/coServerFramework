#include "log.h"
#include "utils/macro.h"
#include "utils/util.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

int main() {
	AZZATO_LOG_INFO(g_logger) << azzato::backtraceToString(10, 2, "");

	int arr[] = {1, 3, 5, 7, 9, 11};
	int n	  = static_cast<int>(sizeof(arr) / sizeof(arr[0]));

	AZZATO_LOG_INFO(g_logger) << "binarySearch 0 -> " << azzato::binarySearch(arr, n, 0);
	AZZATO_LOG_INFO(g_logger) << "binarySearch 1 -> " << azzato::binarySearch(arr, n, 1);
	AZZATO_LOG_INFO(g_logger) << "binarySearch 4 -> " << azzato::binarySearch(arr, n, 4);
	AZZATO_LOG_INFO(g_logger) << "binarySearch 13 -> " << azzato::binarySearch(arr, n, 13);

	assert(azzato::binarySearch(arr, n, 0) == -1);
	assert(azzato::binarySearch(arr, n, 1) == 0);
	assert(azzato::binarySearch(arr, n, 2) == -2);
	assert(azzato::binarySearch(arr, n, 3) == 1);
	assert(azzato::binarySearch(arr, n, 4) == -3);
	assert(azzato::binarySearch(arr, n, 5) == 2);
	assert(azzato::binarySearch(arr, n, 6) == -4);
	assert(azzato::binarySearch(arr, n, 7) == 3);
	assert(azzato::binarySearch(arr, n, 8) == -5);
	assert(azzato::binarySearch(arr, n, 9) == 4);
	assert(azzato::binarySearch(arr, n, 10) == -6);
	assert(azzato::binarySearch(arr, n, 11) == 5);
	assert(azzato::binarySearch(arr, n, 12) == -7);

	AZZATO_LOG_INFO(g_logger) << "test_util over";
	return 0;
}
