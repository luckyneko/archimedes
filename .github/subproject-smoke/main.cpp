#include <archimedes/archimedes.h>

int main()
{
	const acm::Version version = acm::VERSION;
	return version.major == 0 && version.minor == 1 ? 0 : 1;
}
