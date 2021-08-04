/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

/************************************
  * File:   entscheidungsfinder.cpp *
  * Author: gordon                  *
  ***********************************
  * Created on 15. Juni 2021, 03:40 *
  ************************************/
  
#ifdef _MINGW
    #define TICK GetTickCount
#else
    #define TICK GetTickCount64
#endif
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <cmath>
#include <string>
#ifdef _WIN32
  #include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <time.h>

uint64_t GetTickCount64() {
    struct timespec ts;
    uint64_t theTick = 0ULL;
    clock_gettime( CLOCK_REALTIME, &ts );
    theTick  = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;
    return theTick;
}
#endif

using namespace std;

//see https://software.intel.com/content/www/us/en/develop/articles/intel-digital-random-number-generator-drng-software-implementation-guide.html
typedef struct cpuid_struct {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
} cpuid_t;

void cpuid(cpuid_t* info, unsigned int leaf, unsigned int subleaf)
{
#if defined(_WIN32) && !defined(_MINGW)
    unsigned int dwEAX, dwEBX, dwECX, dwEDX;
    __asm {
        pushad
        mov eax, leaf
        mov ecx, subleaf
        cpuid
        mov dwEAX, eax
        mov dwEBX, ebx
        mov dwECX, ecx
        mov dwEDX, edx
        popad
    }
    info->eax = dwEAX;
    info->ebx = dwEBX;
    info->ecx = dwECX;
    info->edx = dwEDX;

#else
    asm volatile("cpuid"
        : "=a" (info->eax), "=b" (info->ebx), "=c" (info->ecx), "=d" (info->edx)
        : "a" (leaf), "c" (subleaf)
        );
#endif
}

int _is_intel_cpu()
{
    cpuid_t info{};
    cpuid(&info, 0, 0);

    return !(memcmp((char*)&info.ebx, "Genu", 4)
        || memcmp((char*)&info.edx, "ineI", 4)
        || memcmp((char*)&info.ecx, "ntel", 4));
}

#define DRNG_NO_SUPPORT	0x0
#define DRNG_HAS_RDRAND	0x1
#define DRNG_HAS_RDSEED	0x2

int get_drng_support()
{
    static int drng_features = -1;

    /* So we don't call cpuid multiple times for
     * the same information */

    if (drng_features == -1) {
        drng_features = DRNG_NO_SUPPORT;

        if (_is_intel_cpu()) {
            cpuid_t info;

            cpuid(&info, 1, 0);

            if ((info.ecx & 0x40000000) == 0x40000000) {
                drng_features |= DRNG_HAS_RDRAND;
            }

            cpuid(&info, 7, 0);

            if ((info.ebx & 0x40000) == 0x40000) {
                drng_features |= DRNG_HAS_RDSEED;
            }
        }
    }

    return drng_features;
}

void benchmark_rd(bool seed);
void info()
{
    bool isIntelCPU = _is_intel_cpu() & 1;
    bool hasRDRAND = get_drng_support() & DRNG_HAS_RDRAND;
    bool hasRDSEED = get_drng_support() & DRNG_HAS_RDSEED;

    printf("An %s CPU was detected.\n", isIntelCPU ? "Intel" : "AMD");
    if (hasRDRAND) {
        printf("Your CPU supports the RDRAND instruction.\n");
    }
    if (hasRDSEED) {
        printf("Your CPU supports the RDSEED instruction.\n");
    }
    if (!hasRDSEED && !hasRDRAND) {
        printf("Your CPU does not support generating pseudo- and/or true- random numbers.\n");
    }

    benchmark_rd(false);
    benchmark_rd(true);
    exit(0);
}

int rdrand32(uint32_t* rand)
{
#if !defined(_WIN32) || defined(_MINGW)
    unsigned char ok;
    asm volatile ("rdrand %0; setc %1"
        : "=r" (*rand), "=qm" (ok));
    return (int)ok;
#else
    __asm {
        push ebx
        rdrand ebx
        jnc here
        mov eax, 1
        push ecx
        mov ecx, rand
        mov dword ptr[ecx], ebx
        pop ecx
        jmp da
        here:
        xor eax, eax
        da:
        pop ebx
    }
#endif
}

int rdseed32(uint32_t* rand)
{
#if !defined(_WIN32) || defined(_MINGW)
    unsigned char ok;
    asm volatile ("rdseed %0; setc %1"
        : "=r" (*rand), "=qm" (ok));
    return (int)ok;
#else
    __asm {
        push ebx
        rdseed ebx
        jnc here
        mov eax, 1
        push ecx
        mov ecx, rand
        mov dword ptr[ecx], ebx
        pop ecx
        jmp da
        here:
        xor eax, eax
        da:
        pop ebx
    }
#endif
}

const int benchmark_time = 5000;
void benchmark_rd(bool seed = false)
{
    typedef int (*rd_t)(uint32_t*);
    rd_t rd = (rd_t)(seed ? rdseed32 : rdrand32);
    uint64_t tick = TICK();
    auto start = tick;
    auto end = tick + benchmark_time;
    uint64_t num_iterations = 0;
    while (TICK() < end) {
        uint32_t buf;
        if (rd(&buf)) {
            ++num_iterations;
        }
    }
    auto endtime = TICK();
    uint64_t generated_bits = num_iterations * 32 / 1000;
    uint64_t real_time_needed_ms = endtime - start;
    uint64_t kbps = generated_bits / (real_time_needed_ms / 1000); //kilobits per second
    uint64_t ips = num_iterations / (real_time_needed_ms / 1000); //iterations per second
    printf("%s: %llu kbps | %llu ips\n", seed ? "RDSEED" : "RDRAND", kbps, ips);
}

int main(int argc, char** argv)
{
    start:
    char topic[64]{};
    printf("-> ");
    fgets(topic, sizeof(topic), stdin);
    *strstr(topic, "\n") = '\0';

    if (!strcmp(topic, "info")) {
        info();
        goto start;
    }

    deque<string> vec;
    char line[64]{};
    bool filled;
    do {
        printf("--> ");
        fgets(line, sizeof(line), stdin);
        filled = line[0] != '\n';
        if (filled) {
            *strstr(line, "\n") = '\0';
            vec.push_back(line);
        }
    } while (filled);

    int choices = vec.size();
    uint32_t number = 0;
    if (_is_intel_cpu()) {
        int rd = get_drng_support();
        //try at maximum 10 times to get a random value if previous generation failed
        int i = 0;
        for (; i < 10; i++) {
            if (rd & (DRNG_HAS_RDRAND | DRNG_HAS_RDSEED)) {
                uint32_t buf;
                if (rdrand32(&number) && rdseed32(&buf)) {
                    number ^= buf;
                    break;
                }
            }
            else if (rd & DRNG_HAS_RDSEED) {
                if (rdseed32(&number))
                    break;
            }
            else if (rd & DRNG_HAS_RDRAND) {
                if (rdrand32(&number))
                    break;
            }
        }
        if (i == 10) {
            goto other_method;
        }
    }
    else {
    other_method:
        srand(time(0) + pow(2, choices));
        number = rand();
    }

    printf("[%s] -> [%s]\n", strlen(topic) ? topic : "(null)", vec[number % choices].c_str());

#ifdef _WIN32
    (void)_getch();
#endif
    return 0;
}
