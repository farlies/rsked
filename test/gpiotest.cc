/**
 * Test the GPIO pins used by rsked on the RPi platform:
 * - red led
 * - green led
 * - button
 * - fan
 * This is a manual test, and requires user interaction to press the
 * button and verify that the lights blink and fan turns on. It does
 * not use the boost log framework.
 */

/*   Part of the rsked package.
 *
 *   Copyright 2020 Steven A. Harp
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include <gpiod.hpp>
#include <iostream>
#include <unistd.h>

#ifndef	CONSUMER
#define	CONSUMER	"gpiotest"
#endif

int main(int, char **)
{
  unsigned int red_line_num = 27;
  unsigned int grn_line_num = 17;
  unsigned int btn_line_num = 18;
  unsigned int fan_line_num = 4;

  try {
    gpiod::chip chip("gpiochip0");
    gpiod::line_request config_o {CONSUMER,gpiod::line_request::DIRECTION_OUTPUT,0};
    gpiod::line_request config_i {CONSUMER,gpiod::line_request::DIRECTION_INPUT,0};
    gpiod::line red_line = chip.get_line( red_line_num );
    gpiod::line grn_line = chip.get_line( grn_line_num );
    gpiod::line btn_line = chip.get_line( btn_line_num );
    gpiod::line fan_line = chip.get_line( fan_line_num );
    fan_line.request( config_o, 0 );
    red_line.request( config_o, 0 );
    grn_line.request( config_o, 0 );
    btn_line.request( config_i );

    std::cout << "Starting Fan\n";
    fan_line.set_value(1);
    /* Blink either the red or green LED based on button position */
    unsigned int rval = 0, gval=0;
    for (int i = 20; i > 0; i--) {
      if (btn_line.get_value()) {
	std::cout << "set red to " << rval << '\n';
	red_line.set_value( rval );
	sleep(1);
	rval = (1 - rval);
      }
      else {
	std::cout << "set green to " << gval << '\n';
	grn_line.set_value( gval );
	sleep(1);
	gval = (1 - gval);
      }
    }
    std::cout << "Turn off all output pins.\n";
    red_line.set_value(0);
    grn_line.set_value(0);
    fan_line.set_value(0);
    //
  } catch( const std::exception &xn ) {
    std::cerr << "gpio exception: " << xn.what() << '\n';
    return 1;
  }
  return 0;
}
