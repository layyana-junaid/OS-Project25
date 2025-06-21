# Multithreaded File Downloader with Verification

**A 4th Semester Operating Systems Project**  

![Capture](https://github.com/user-attachments/assets/182127af-6950-47cb-913d-0dbf8a10322a)


## Key Features
- 🚀 **Parallel Downloads**: 4 threads (`NUM_THREADS`) fetch file segments simultaneously using HTTP range requests
- 🔒 **Thread-Safe Buffer**: Bounded circular buffer (`BUFFER_SIZE=8`) with pthread synchronization
- ✔️ **Data Verification**: 2 dedicated threads (`NUM_READERS`) validate segments using Adler-32 checksums
- 📊 **Real-time UI**: GTK interface showing download progress and verification status
- 💾 **Ordered Writing**: Consumer thread assembles verified segments sequentially

## Technical Highlights
| Component              | Implementation Details |
|------------------------|------------------------|
| **Concurrency Control** | Mutexes + Condition Variables + RW Locks |
| **Network Layer**       | libcURL with range requests |
| **Synchronization**     | Producer → Buffer → Verifier → Consumer pipeline |
| **Error Handling**      | Thread-safe error propagation to UI |

## Build & Run
### Prerequisites
- GTK3 development libraries
- libcurl
- POSIX-compliant system (Linux/macOS)

```bash
# Ubuntu/Debian
sudo apt install build-essential libgtk-3-dev libcurl4-openssl-dev

# Compile
gcc -o downloader main.c downloader.c ui.c -lpthread -lcurl `pkg-config --cflags --libs gtk+-3.0`

# Run
./downloader
![image](https://github.com/user-attachments/assets/21e412bf-52a3-448c-bc57-d07407977965)


### Running implementation of MUltithreaded File Downloader:
https://github.com/user-attachments/assets/37421f43-49f7-41a4-853a-e453ae192185

