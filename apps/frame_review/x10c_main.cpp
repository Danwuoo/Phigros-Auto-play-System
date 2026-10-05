#include "contact_replay.hpp"
#include <iostream>
int main(int argc,char** argv){if(argc<2){std::cerr<<"contact command required\n";return 1;}return contact_replay_main(argc,argv);}
