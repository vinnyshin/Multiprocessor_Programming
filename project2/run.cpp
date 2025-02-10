#include <iostream>
#include "snapshot.hpp"

using namespace std;
 
int main(int argc, char const *argv[]) {
    int num_of_threads = stoi(argv[1]);
    
    Snapshot Snapshot(num_of_threads);

    size_t cnt = Snapshot.run();
    cout << cnt << endl;
    
    return 0;
}
