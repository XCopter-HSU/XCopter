-- pwm.vhd

-- This file was auto-generated as a prototype implementation of a module
-- created in component editor.  It ties off all outputs to ground and
-- ignores all inputs.  It needs to be edited to make it do something
-- useful.
-- 
-- This file will not be automatically regenerated.  You should check it in
-- to your version control system if you want to keep it.

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity pwm is
	port (
		--inputs (avalon MM slave)
		clk              : in  std_logic                     := '0';             --       clock.clk
		reset            : in  std_logic                     := '0';             --            .reset
		avs_s0_write     : in  std_logic                     := '0';             --          s0.write
		avs_s0_writedata : in  std_logic_vector(31 downto 0) := (others => '0'); --            .writedata
		--output (conduit)
		out_signal       : out std_logic                                         -- conduit_end.export
	);
end entity pwm; 

architecture rtl of pwm is
	--temporary signals
	signal counter : std_logic_vector(7 downto 0);
	signal data_out : std_logic;
	signal divider : std_logic_vector(9 downto 0); --to adjust the frequency (10bit for ~200Hz)
begin
	process (clk, reset)
	variable current_data : std_logic_vector(7 downto 0);
	begin
		if reset = '1' then
			data_out <= '0';
		elsif clk'event and clk = '1' then
			--cache writedata in current_data so that it doesn't get lost
			if avs_s0_write ='1' then
				current_data  := avs_s0_writedata(7 downto 0);
			end if;		
			--divider to adjust frequency
			if divider = "0000001100" then	
				divider <= "0000000000";
				--(input) compare current_data with counter to generate pwm out_signal
				if counter > current_data OR current_data = "00000000" then
					data_out <= '0';
				else
					data_out <= '1';
				end if;
				
				--counter incrementation and reset
				if counter = "11111111" then
					counter <= "00000000";
				else
					counter <= counter + 1;	
				end if;
			--increment divider
			else
				divider <= divider +1;
			end if;
		end if;
		
	end process;

	out_signal <= data_out;
end architecture rtl; -- of pwm