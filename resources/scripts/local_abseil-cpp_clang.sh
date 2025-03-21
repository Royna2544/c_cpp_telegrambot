set -e

LATEST_ABSEIL_CPP_LTS=20250127.1

git clone https://github.com/abseil/abseil-cpp abseil-cpp --tags
trap "rm -rf abseil-cpp" EXIT INT

cd abseil-cpp
git checkout $LATEST_ABSEIL_CPP_LTS
cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_FLAGS=-fclang-abi-compat=17 -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . -B build 
cmake --build build -- -j$(nproc)
sudo cmake --build build --target install -- -j$(nproc)