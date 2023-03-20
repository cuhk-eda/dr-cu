tar --bzip2 -xf boost_1_67_0.tar.bz2
mkdir ../boost
cd boost_1_67_0
./bootstrap.sh --prefix=../../boost
./b2 install --prefix=../../boost
