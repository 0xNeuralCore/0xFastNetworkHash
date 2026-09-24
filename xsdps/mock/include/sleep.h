#ifndef XSDPS_MOCK_SLEEP_H
#define XSDPS_MOCK_SLEEP_H

static inline int usleep(unsigned int Microseconds)
{
	(void)Microseconds;
	return 0;
}

#endif /* XSDPS_MOCK_SLEEP_H */
