all: a.out

a.out: main.o kb_rand.o boost_static.o
  g++ -O3 -s main.o kb_rand.o boost_static.o -lpthread

main.o: main.cpp
  g++ -O3 -c -std=c++17 main.cpp
  
kb_rand.o: kb_rand.cpp
  g++ -O3 -c -std=c++17 kb_rand.cpp

boost_static.o: boost_static.cpp
  g++ -O3 -c -std=c++17 boost_static.cpp
