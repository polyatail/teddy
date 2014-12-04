#include <stdio.h>
#include <cstdlib>
#include <unistd.h>

int main()
{
  double freq[] = {300, 310};

  while (1)
  {
    freq[0] = ((float)rand() / (float)RAND_MAX) * 440 + 220;
    freq[1] = freq[0] + 5;//((float)rand() / (float)RAND_MAX) * 440 + 220;

    fwrite(freq, sizeof(double), 2, stdout);
    fflush(stdout);
    usleep(300000 * (float)rand() / (float)RAND_MAX + 50000);
  }

  return 0;
}
