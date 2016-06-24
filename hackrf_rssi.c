/*
    Measure RSSI using HackRF and liquid-dsp
*/

#include <libhackrf/hackrf.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include <liquid/liquid.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <signal.h>

static hackrf_device* device = NULL;

static const int samplestoaverage = 1024;

int verbose = 0;
float numSeconds = 5.0;
uint64_t frequency = 88600000;
uint32_t lnagain = 32;
uint32_t vgagain = 44;
double bandwidth = 1000000;

int doneReceiving = 0;

static void usage() {
  // print help/usage info
  printf("Syntax: hackrf_rssi\r\n");
  printf("-f [freq Hz] \t Set frequency\r\n");
  printf("-b [band Hz] \t Set bandwidth\r\n");
  printf("-g [gain dB] \t Set VGA gain\r\n");
  printf("-l [gain dB] \t Set LNA gain\r\n");

}

void intHandler() {
    doneReceiving = 1;
}

int rx_callback(hackrf_transfer* transfer) {
  // rx_callback() called every time new data arrives from HackRF

  // Time domain samples go in samples[], output FFT will be in fft[]
  float complex raw_samples[1024];
  float complex samples[1024];
  float complex agc_output;

  // Create IIR filter to remove DC offset
  float alpha = 0.10f;
  iirfilt_crcf dcblock = iirfilt_crcf_create_dc_blocker(alpha);

  // Setup automatic gain control
  agc_crcf agc_rx = agc_crcf_create();
  agc_crcf_set_bandwidth(agc_rx, 0.01f);

  // Receive raw I and Q samples from HackRF, and put into complex array
  for(int i = 0; i<samplestoaverage; i++) {
    raw_samples[i] = transfer->buffer[2*i] + transfer->buffer[2*i + 1]*I; // Convert to complex
    // raw_samples[i] = (raw_samples[i])*(1.0/128.0); // Scale
    iirfilt_crcf_execute(dcblock, raw_samples[i], &samples[i]); // Remove DC offset
    agc_crcf_execute(agc_rx, samples[i], &agc_output); // Execute AGC
  }
  printf("Current RSSI: -%f dBm\r\n", agc_crcf_get_rssi(agc_rx));

  // Destroy objects
  iirfilt_crcf_destroy(dcblock);
  agc_crcf_destroy(agc_rx);

  return 0;
}

int main(int argc, char **argv) {

  // Setup variables from arguments
  int d;
  while ((d = getopt(argc,argv,"hvqf:b:t:G:L:o:")) != EOF) {
      switch (d) {
      case 'h':   usage();                        return 0;
      case 'v':   verbose = 1;                    break;
      case 'f':   frequency = atof(optarg);       break;
      case 'b':   bandwidth = atof(optarg);       break;
      case 't':   numSeconds = atof(optarg);      break;
      case 'g':   vgagain = atof(optarg);         break;
      case 'l':   lnagain = atoi(optarg);         break;
      default:
          return 1;
      }
  }

  int result;

  // libhackrf must be initialized first
  result = hackrf_init();
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_init() failed: %s (%d)\n", hackrf_error_name(result), result);
    usage();
    return EXIT_FAILURE;
  }

  // attempt to open first HackRF device seen
  result = hackrf_open(&device);
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_open() failed: %s (%d)\n", hackrf_error_name(result), result);
    usage();
    return EXIT_FAILURE;
  }

  // set freq gain and bandwidth
  if(verbose) printf("Setting frequency to %llu Hz...", frequency);
  result = hackrf_set_freq(device, frequency);
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }
  result = hackrf_set_lna_gain(device, lnagain);
  result |= hackrf_set_vga_gain(device, vgagain);
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_set_gains failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }
  result = hackrf_set_baseband_filter_bandwidth(device, hackrf_compute_baseband_filter_bw(bandwidth));
  result = hackrf_set_sample_rate(device, bandwidth);
  if( result != HACKRF_SUCCESS ) {
    printf("Setting bandwidth failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  }

  // start receiving, call rx_callback() when data received
  if(verbose) printf("Starting reception...\r\n");
  result = hackrf_start_rx(device, rx_callback, NULL);
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_start_rx() failed: %s (%d)\n", hackrf_error_name(result), result);
    return EXIT_FAILURE;
  } else {
    if(verbose) printf("hackrf_start_rx() done.\r\n");
  }

  signal(SIGINT, intHandler); // Close properly if Ctrl+C

  doneReceiving = 0;
  clock_t start = clock(), runtime;
  while(doneReceiving==0){
    // wait numSeconds before stopping reception
    runtime = (clock() - start) * 1000 / CLOCKS_PER_SEC;
    if(runtime >= numSeconds*1000)
      doneReceiving = 1;
  }

  // stop receiving
  result = hackrf_stop_rx(device);
  if( result != HACKRF_SUCCESS ) {
    printf("hackrf_stop_rx() failed: %s (%d)\n", hackrf_error_name(result), result);
  }else {
    if(verbose) printf("hackrf_stop_rx() done\n");
  }

  // close device
  result = hackrf_close(device);
  if( result != HACKRF_SUCCESS )
  {
    printf("hackrf_close() failed: %s (%d)\n", hackrf_error_name(result), result);
  }else {
    if(verbose) printf("hackrf_close() done\n");
  }

  hackrf_exit();
  if(verbose) printf("hackrf_exit() done\n");

  return EXIT_SUCCESS;
}
