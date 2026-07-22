#include "application.h"

#include <cstdlib>
#include <ctime>

int main(int argc, char** argv) {
	srand(static_cast<unsigned>(time(nullptr)));
	azzato::Application app;
	if(app.init(argc, argv)) {
		return app.run() ? 0 : 1;
	}
	return 0;
}
