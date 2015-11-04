library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity CPU_FiFo_Bridge is
	port (
		clk            : in  std_logic                     := '0';             -- clock.clk
		reset          : in  std_logic                     := '0';             --      .reset
		cpu1_address   : in  std_logic_vector(7 downto 0)  := (others => '0'); --    s0.address
		cpu1_read      : in  std_logic                     := '0';             --      .read
		cpu1_readdata  : out std_logic_vector(31 downto 0);                    --      .readdata
		cpu1_write     : in  std_logic                     := '0';             --      .write
		cpu1_writedata : in  std_logic_vector(31 downto 0) := (others => '0'); --      .writedata
		cpu1_irq       : out std_logic;                                        --  irq0.irq
		cpu2_address   : in  std_logic_vector(7 downto 0)  := (others => '0'); --    s1.address
		cpu2_read      : in  std_logic                     := '0';             --      .read
		cpu2_readdata  : out std_logic_vector(31 downto 0);                    --      .readdata
		cpu2_writedata : in  std_logic_vector(31 downto 0) := (others => '0'); --      .writedata
		cpu2_write     : in  std_logic                     := '0';             --      .write
		cpu2_irq       : out std_logic                                         --  irq1.irq
	);
end entity CPU_FiFo_Bridge;

architecture rtl of CPU_FiFo_Bridge is

	signal cpu1_readData_signal				: std_logic_vector (31 downto 0);
	signal cpu1_cpu2_fifo_clear				: std_logic ;
	signal cpu1_cpu2_fifo_wrreq				: std_logic ;
	signal cpu1_cpu2_fifo_rdreq				: std_logic ;
	signal cpu1_cpu2_fifo_readData			: std_logic_vector (31 downto 0);

	signal cpu1_cpu2_fifo_full				: std_logic;
	signal cpu1_cpu2_fifo_almostfull		: std_logic;
	signal cpu1_cpu2_fifo_almostempty		: std_logic;
	signal cpu1_cpu2_fifo_empty				: std_logic;
	signal cpu1_cpu2_fifo_count				: std_logic_vector (4 downto 0);
	signal cpu1_cpu2_fifo_count_out			: std_logic_vector (5 downto 0);


	signal cpu2_readData_signal				: std_logic_vector (31 downto 0);
	signal cpu2_cpu1_fifo_clear				: std_logic ;
	signal cpu2_cpu1_fifo_wrreq				: std_logic ;
	signal cpu2_cpu1_fifo_rdreq				: std_logic ;
	signal cpu2_cpu1_fifo_readData			: std_logic_vector (31 downto 0);
	
	signal cpu2_cpu1_fifo_full				: std_logic;
	signal cpu2_cpu1_fifo_almostfull		: std_logic;
	signal cpu2_cpu1_fifo_almostempty		: std_logic;
	signal cpu2_cpu1_fifo_empty				: std_logic;
	signal cpu2_cpu1_fifo_count				: std_logic_vector (4 downto 0);
	signal cpu2_cpu1_fifo_count_out			: std_logic_vector (5 downto 0);
	
	
	
	signal statusRegister						: std_logic_vector (7 downto 0);	-- Bit 0 ( 1 = Sende-FIFO ist leer )
																					-- Bit 1 ( 1 = Sende-FIFO ist fast leer )
																					-- Bit 2 ( 1 = Sende-FIFO ist fast voll )
																					-- Bit 3 ( 1 = Sende-FIFO ist voll )
																					-- Bit 4 ( 1 = Empfangs-FIFO ist leer )
																					-- Bit 5 ( 1 = Empfangs-FIFO ist fast leer )
																					-- Bit 6 ( 1 = Empfangs-FIFO ist fast voll )
																					-- Bit 7 ( 1 = Empfangs-FIFO ist voll )
																				
																				
																				
	signal cpu1_interruptRegister				: std_logic_vector (7 downto 0);	-- Bit 0 ( 1 = FiFo_cpu1_cpu2 ist fast leer )
																					-- Bit 1 ( 1 = FiFo_cpu2_cpu1 ist fast voll )
																												
	signal cpu1_interruptEnableRegister			: std_logic_vector (7 downto 0);	-- Bit 0 ( 1 = FiFo_cpu1_cpu2 ist fast leer )
																					-- Bit 1 ( 1 = FiFo_cpu2_cpu1 ist fast voll )
																					
																																									
	
	signal cpu2_interruptRegister				: std_logic_vector (7 downto 0);	-- Bit 0 ( 1 = FiFo_cpu2_cpu1 ist fast leer )
																					-- Bit 1 ( 1 = FiFo_cpu1_cpu2 ist fast voll )
																					
	signal cpu2_interruptEnableRegister			: std_logic_vector (7 downto 0);	-- Bit 0 ( 1 = FiFo_cpu2_cpu1 ist fast leer )
																					-- Bit 1 ( 1 = FiFo_cpu1_cpu2 ist fast voll )
																					
																					


	signal fueller 								: std_logic_vector(23 downto 0) := "000000000000000000000000";
	
	
	signal cpu1_interruptRegister_last : std_logic_vector (7 downto 0);
	signal cpu2_interruptRegister_last : std_logic_vector (7 downto 0);
	
	
	-- Komponentendeklaration des FiFos
	component fifo is
		port(	clock			: in std_logic ;
				data			: in std_logic_vector (31 DOWNTO 0);
				rdreq			: in std_logic ;
				sclr			: in std_logic ;
				wrreq			: in std_logic ;
				almost_empty	: out std_logic ;
				almost_full		: out std_logic ;
				empty			: out std_logic ;
				full			: out std_logic ;
				q				: out std_logic_vector (31 DOWNTO 0);
				usedw			: out std_logic_vector (4 DOWNTO 0));
	end component fifo;

begin

	fifo_cpu1_to_cpu2 : fifo port map (
		clock	 		=> clk,
		data	 		=> cpu1_writedata,
		rdreq	 		=> cpu1_cpu2_fifo_rdreq,
		sclr	 		=> cpu1_cpu2_fifo_clear,
		wrreq	 		=> cpu1_cpu2_fifo_wrreq,
		almost_empty	=> cpu1_cpu2_fifo_almostempty,
		almost_full	 	=> cpu1_cpu2_fifo_almostfull,
		empty	 		=> cpu1_cpu2_fifo_empty,
		full	 		=> cpu1_cpu2_fifo_full,
		q	 			=> cpu1_cpu2_fifo_readData,
		usedw	 		=> cpu1_cpu2_fifo_count
	);
	
	fifo_cpu2_to_cpu1 : fifo port map (
		clock	 		=> clk,
		data	 		=> cpu2_writedata,
		rdreq	 		=> cpu2_cpu1_fifo_rdreq,
		sclr	 		=> cpu2_cpu1_fifo_clear,
		wrreq	 		=> cpu2_cpu1_fifo_wrreq,
		almost_empty	=> cpu2_cpu1_fifo_almostempty,
		almost_full	 	=> cpu2_cpu1_fifo_almostfull,
		empty	 		=> cpu2_cpu1_fifo_empty,
		full	 		=> cpu2_cpu1_fifo_full,
		q	 			=> cpu2_cpu1_fifo_readData,
		usedw	 		=> cpu2_cpu1_fifo_count
	);


	-- Prozess zum reseten der Fifos und zum Setzen der IEnable Register
	process (clk, reset)
	begin
		if (reset = '1') then
			cpu1_cpu2_fifo_clear <= '1';
			cpu2_cpu1_fifo_clear <= '1';
			cpu1_interruptEnableRegister <= "00000000";
			cpu2_interruptEnableRegister <= "00000000";
		else
			if rising_edge(clk) then
				if (cpu1_address = "00000001" and cpu1_write = '1') then
					cpu1_interruptEnableRegister <= cpu1_interruptEnableRegister XOR cpu1_writedata(7 downto 0);
				end if;
				
				if (cpu2_address = "00000001" AND cpu2_write = '1') then
					cpu2_interruptEnableRegister <= cpu2_interruptEnableRegister XOR cpu2_writedata(7 downto 0);
				end if;
				
				if (cpu1_address = "00000011" AND cpu1_write = '1' AND cpu1_writedata(0) = '1') then
					cpu1_cpu2_fifo_clear <= '1';
				else
					cpu1_cpu2_fifo_clear <= '0';
				end if;
				
				if (cpu2_address = "00000011" AND cpu2_write = '1' AND cpu2_writedata(0) = '1') then
					cpu2_cpu1_fifo_clear <= '1';
				else
					cpu2_cpu1_fifo_clear <= '0';
				end if;
			end if;
		end if;
	end process;
	
	-- Prozess zur Verarbeitung und Definition der Interruptregister von CPU1
	interrupt_process_cpu1 : process (clk, reset)
	begin
		if (reset = '1') then
			cpu1_interruptRegister <= "00000000";
			cpu1_interruptRegister_last <= "00000000";
		elsif (rising_edge(clk)) then
			if (cpu1_address = "00000010" AND cpu1_write = '1') then
				cpu1_interruptRegister <= cpu1_interruptRegister XOR (cpu1_interruptRegister AND cpu1_writedata(7 downto 0));
			end if;
			
			if (cpu1_cpu2_fifo_almostempty = '1' AND cpu1_interruptRegister_last(0) = '0' AND cpu1_interruptEnableRegister(0) = '1') then
				cpu1_interruptRegister(0) <= '1';
			end if;
			if (cpu2_cpu1_fifo_almostfull = '1' AND cpu1_interruptRegister_last(1) = '0' AND cpu1_interruptEnableRegister(1) = '1') then
				cpu1_interruptRegister(1) <= '1';
			end if;
			
			cpu1_interruptRegister_last <= cpu1_interruptRegister;
		end if;
	end process;
	
	-- Prozess zur Verarbeitung und Definition der Interruptregister von CPU2
	interrupt_process_cpu2 : process (clk, reset)
	begin
		if (reset = '1') then
			cpu2_interruptRegister <= "00000000";
			cpu2_interruptRegister_last <= "00000000";
		elsif (rising_edge(clk)) then
			if (cpu2_address = "00000010" AND cpu2_write = '1') then
				cpu2_interruptRegister <= cpu2_interruptRegister XOR (cpu2_interruptRegister AND cpu2_writedata(7 downto 0));
			end if;
			
			if (cpu2_cpu1_fifo_almostempty = '1' AND cpu2_interruptRegister_last(0) = '0' AND cpu2_interruptEnableRegister(0) = '1') then
				cpu2_interruptRegister(0) <= '1';
			end if;
			if (cpu1_cpu2_fifo_almostfull = '1' AND cpu2_interruptRegister_last(1) = '0' AND cpu2_interruptEnableRegister(1) = '1') then
				cpu2_interruptRegister(1) <= '1';
			end if;
			
			cpu2_interruptRegister_last <= cpu2_interruptRegister;
		end if;
	end process;
	
	
	-- Statusregister mit aktuellen Werten füllen
	statusRegister(0) <= cpu1_cpu2_fifo_empty;
	statusRegister(1) <= cpu1_cpu2_fifo_almostempty;
	statusRegister(2) <= cpu1_cpu2_fifo_almostfull;
	statusRegister(3) <= cpu1_cpu2_fifo_full;
	statusRegister(4) <= cpu2_cpu1_fifo_empty;
	statusRegister(5) <= cpu2_cpu1_fifo_almostempty;
	statusRegister(6) <= cpu2_cpu1_fifo_almostfull;
	statusRegister(7) <= cpu2_cpu1_fifo_full;


	-- Steuerleitungen der Fifos setzen
	cpu2_cpu1_fifo_rdreq <= '1' when cpu1_read = '1' and cpu1_address = "00000000" else
							'0';
								
	cpu1_cpu2_fifo_rdreq <= '1' when cpu2_read = '1' and cpu2_address = "00000000" else
							'0';
							
							
	cpu1_cpu2_fifo_wrreq <= '1' when cpu1_write = '1' and cpu1_address = "00000000" else
							'0';
	
	cpu2_cpu1_fifo_wrreq <= '1' when cpu2_write = '1' and cpu2_address = "00000000" else
							'0';

							
	-- Füllstand setzen
	cpu2_cpu1_fifo_count_out <= ("0" & cpu2_cpu1_fifo_count) when cpu2_cpu1_fifo_full = '0' else
								"100000";
						
	cpu1_cpu2_fifo_count_out <= ("0" & cpu1_cpu2_fifo_count) when cpu1_cpu2_fifo_full = '0' else
								"100000";						
				
	-- Avalon-Datenleitung je nach gewählter Adresse füllen
	cpu1_readdata <= 	cpu2_cpu1_fifo_readData														when cpu1_address = "00000000" else
						(fueller & cpu1_interruptEnableRegister)									when cpu1_address = "00000001" else
						(fueller & cpu1_interruptRegister)											when cpu1_address = "00000010" else
						(fueller & statusRegister)													when cpu1_address = "00000011" else
						(fueller & "00" & cpu2_cpu1_fifo_count_out) 								when cpu1_address = "00000100" else
						(fueller & "00" & cpu1_cpu2_fifo_count_out) 								when cpu1_address = "00000101" else
						(fueller & "11111111");

	cpu2_readdata <= 	cpu1_cpu2_fifo_readData														when cpu2_address = "00000000" else
						(fueller & cpu2_interruptEnableRegister)									when cpu2_address = "00000001" else
						(fueller & cpu2_interruptRegister)											when cpu2_address = "00000010" else
						(fueller & statusRegister(3 downto 0) & statusRegister(7 downto 4) )		when cpu2_address = "00000011" else
						(fueller & "00" & cpu1_cpu2_fifo_count_out) 								when cpu2_address = "00000100" else
						(fueller & "00" & cpu2_cpu1_fifo_count_out)									when cpu2_address = "00000101" else
						(fueller & "11111111");


	-- Interrupt setzen
	cpu1_irq <= '0' when cpu1_interruptRegister = "00000000" else
				'1';
				
	cpu2_irq <= '0' when cpu2_interruptRegister = "00000000" else
				'1';

end architecture rtl; -- of CPU_FiFo_Bridge
