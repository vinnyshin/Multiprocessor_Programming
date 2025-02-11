# Multiprocessor Programming

## 1. Parallel Join Processing
This project enhances the performance of sequential join processing by introducing Inter-query and Intra-query parallelism strategies. By implementing these optimizations, the execution time was reduced from 193.8 seconds to 109 seconds, achieving a 43.8% performance improvement.

## [2. Atomic snapshot](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project2/documents/Atomic%20snapshot.md)
This project implements a wait-free atomic snapshot algorithm based on the approach described in The Art of Multiprocessor Programming. The implementation ensures a highly efficient snapshot mechanism that prevents blocking and improves system responsiveness.

## [3. Improe the BW-Tree](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project3/documents/milestone%203.md)
This project involves analyzing the open-source BW-Tree and identifying performance bottlenecks. By diagnosing critical issues and validating their impact, the project introduces improvements to enhance the tree’s efficiency and concurrency management.

## [4. Border-collie on PostgreSQL](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project4/Project4.md)
This project implements the wait-free logging-boundary-finding algorithm (Border-Collie) in PostgreSQL. Although existing locks were not removed, simply applying the Border-Collie approach resulted in noticeable performance improvements.
