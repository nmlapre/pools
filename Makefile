pool: pool.h test_pool.cpp
	g++-11 -g3 test_pool.cpp -o pool

perf:
	g++-11 -O3 -flto test_pool.cpp -o perf_pool

asan:
	g++-11 -g3 -fsanitize=address test_pool.cpp -o asan_pool

all: pool perf asan

clean:
	rm ./pool
	rm ./asan_pool
	rm ./perf_pool
