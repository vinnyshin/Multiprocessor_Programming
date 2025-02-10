# Project4

You can find the code here ''



## Border-Collie: A Wait-free, Read-optimal Algorithm for Database Logging on Multicore Hardware

> Archiving database logs, whether being command or physical logging, needs two steps; 1) recording events in a log buffer and 2) flushing the buffer to stable storage in order. The concept of total ordering on events has pervaded database designers for almost four decades in that full-fledged relational database systems nowadays rely on centralized logging that uses a single log sequencer, with the write-ahead logging (WAL) being flexibly relaxed to trade durability for performance. - (Jongbin Kim, et al., 2019)



Modern database systems still suffer performance bottlenecks owing to database logging.

Border-Collie addresses the logging bottleneck based on a wait-free, read-optimal algorithm that scales the performance of logging better than state-of-the-art techniques.



![Lecture12-advanced_logging_algorithm_19](/Users/vinny/Desktop/Lecture12-advanced_logging_algorithm_19.png)

Border-Collie gives reserved log buffer space using an atomic fetch-and-add instruction to each transaction so that they can make concurrent write on the central log buffer.



![Lecture12-advanced_logging_algorithm_20](/Users/vinny/Desktop/Lecture12-advanced_logging_algorithm_20.png)

Border-Collie then flushes the central log buffer to the Recoverable Logging Boundary (RLB).

RLB is the maximum boundary that we can guarantee to fully recover when the database is crashed. 

Border-Collie finds stragglers by getting the color of the flag.

![](/Users/vinny/Desktop/Lecture12-advanced_logging_algorithm_89.png)

![Lecture12-advanced_logging_algorithm_90](/Users/vinny/Desktop/Lecture12-advanced_logging_algorithm_90.png)

![Lecture12-advanced_logging_algorithm_91](/Users/vinny/Desktop/Lecture12-advanced_logging_algorithm_91.png)



## Design

```c
// xlog.c

typedef enum Color {Black, White} Color;

typedef struct
{
	LWLock		lock;
	XLogRecPtr	insertingAt;
	XLogRecPtr	lastImportantAt;
	
	Color flag;
	XLogRecPtr startPos;
	XLogRecPtr endPos;
} WALInsertLock;

```

- Added flag, startPos, endPos variable
- startPos points to the start address of the log block
- endPos points to the end address of the log block



```c
// xlog.c
XLogRecPtr
XLogInsertRecord(XLogRecData *rdata,
				 XLogRecPtr fpw_lsn,
				 uint8 flags,
				 int num_fpi,
				 bool topxid_included)
{
  // ...
	
  /*
	 * Done! Let others know that we're finished.
	 */
	WALInsertLocks[MyLockNo].l.flag = White;
	WALInsertLockRelease();
  
  // ...
}
```

- Once insertion is finished, change the lock flag color to white.
  - it means that we're done.



```c
// xlog.c

static void
ReserveXLogInsertLocation(int size, XLogRecPtr *StartPos, XLogRecPtr *EndPos,
						  XLogRecPtr *PrevPtr) {
	// ...  
  SpinLockRelease(&Insert->insertpos_lck);
  
  WALInsertLocks[MyLockNo].l.startPos = startbytepos;
  WALInsertLocks[MyLockNo].l.endPos = endbytepos;
	WALInsertLocks[MyLockNo].l.flag = Black;

  *StartPos = XLogBytePosToRecPtr(startbytepos);
	*EndPos = XLogBytePosToEndRecPtr(endbytepos);
	*PrevPtr = XLogBytePosToRecPtr(prevbytepos);
	// ...
}
```

- Before starting to insert, save the startPos and endPos
- Change the lock flag color to black.
  - it means that we're working on the log record.



```c
// xlog.c
bool
XLogBackgroundFlush(void)
{
  // ...
	XLogRecPtr RLB = UINT64_MAX;
	XLogRecPtr cutoff = WriteRqst.Write;

	for (size_t i = 0; i < NUM_XLOGINSERT_LOCKS; ++i) {
		if (WALInsertLocks[i].l.flag == Black) {
			RLB = Min(WALInsertLocks[i].l.startPos, RLB);
		} else if (WALInsertLocks[i].l.flag == White) {
			RLB = Min(Max(cutoff, WALInsertLocks[i].l.endPos), RLB);
		}
	}
  
	WriteRqst.Write = RLB; // newRLB = minRLB;
  // ...
}
```

- Set cutoff to gLSN
- Iterate all lock variable
  - if the flag color is black, it implies that the target transaction is working on the log
  - if the flag color is white, it implies that the target transaction finished with its work
- Set newRLB with minRLB.



## Results

The central log buffer is the main bottleneck point because it has a huge big lock on it.

Lacking time, I skipped making the central log buffer lock-free.



But still, I thought that there could be a performance increase with the RLB mechanism.

The XLogBackgroundFlush can make many flushes compared to the vanilla version.

With minRLB, when it awakes, it will wait a small amount of time and flushes it right away.

It implies that there gonna be a few switches and checkpoints, we don't need to hold the entire 8 locks to prevent further insertion. (When we do switch and checkpoint we need to hold the entire 8 locks)



By reducing switches and checkpoints which are invoked by the full WAL buffer, we can get more throughput.

However, the XLogBackgroundFlush will call more fsync calls.

if the fsync call overhead exceeds the benefit we get, the performance will decrease.



To see the difference,  I did 5 times experiments each with vanilla and Border-Collie and took an average of the results.



### Environment

OS: Linux 22.04.1 LTS

Memory: 32GB

Processor: AMD Ryzen 9 5950X 16-Core Processor (32 threads)

Number of threads: 32

|                           Vanilla                            |                        Border-Collie                         |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| Throughput:<br/>    events/s (eps):                      194531.4331<br/>    time elapsed:                        300<br/>    total number of events:              58359429.93<br/> | Throughput:<br/>    events/s (eps):                      205970.7958<br/>    time elapsed:                        300<br/>    total number of events:              61791238.74<br/> |

It seems that frequent fsync with minRLB mechanism successfully compensates fsync overhead.

we get 5.88% performance increase compared to the vanilla one.

if we can eliminate WAL buffer big latch, we can get more performance increase.