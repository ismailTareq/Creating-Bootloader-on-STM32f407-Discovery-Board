#include "Bootloader.h"
static void Bootloader_Get_Version(uint8_t *Host_Buffer);
static void Bootloader_Get_Help(uint8_t *Host_Buffer);
static void Bootloader_Get_Chip_Identification_Number(uint8_t *Host_Buffer);
static void Bootloader_Read_Protection_Level(uint8_t *Host_Buffer);
static void Bootloader_Jump_To_Address(uint8_t *Host_Buffer);
static void Bootloader_Erase_Flash(uint8_t *Host_Buffer);
static void Bootloader_Memory_Write(uint8_t *Host_Buffer);
static void Bootloader_Change_Read_Protection_Level(uint8_t *Host_Buffer);

static uint8_t Bootloader_CRC_Verify(uint8_t *pData, uint32_t Data_Len, uint32_t Host_CRC);
static void Bootloader_Send_ACK(uint8_t Replay_Len);
static void Bootloader_Send_NACK(void);
static void Bootloader_Send_Data_To_Host(uint8_t *Host_Buffer, uint32_t Data_Len);
static uint8_t Host_Address_Verification(uint32_t Jump_Address);
static uint8_t Perform_Flash_Erase(uint8_t SectorNumber, uint8_t NumberOfSectors);
static uint8_t Flash_Memory_Write_Payload(uint8_t *Host_Payload, uint32_t Payload_Start_Address, uint16_t Payload_Len);
static uint8_t Change_ROP_Level(uint32_t ROP_Level);
static uint8_t CBL_STM32F407_Get_RDP_Level();
static uint8_t Bootloader_Supported_Commands[12] = {
		
		CBL_GET_VER_CMD,
    CBL_GET_HELP_CMD,
    CBL_GET_CID_CMD,
    CBL_GET_RDP_STATUS_CMD,
    CBL_GO_TO_ADDR_CMD,
    CBL_FLASH_ERASE_CMD,
    CBL_MEM_WRITE_CMD,
    CBL_ED_W_PROTECT_CMD,
    CBL_MEM_READ_CMD,
    CBL_READ_SECTOR_STATUS_CMD,
    CBL_OTP_READ_CMD,
    CBL_CHANGE_ROP_Level_CMD

}; 

static uint8_t BL_Host_Buffer[BL_HOST_BUFFER_RX_LENGTH];

BL_Status BL_UART_Fetch_Host_Command(void)
{
	BL_Status Status = BL_NACK;
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	
	uint32_t DataLength;
	//clear buffer to receive from Host
	memset(BL_Host_Buffer,0,BL_HOST_BUFFER_RX_LENGTH);
	
	//Read the length of the command packet received from the Host
	HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_UART, BL_Host_Buffer, 1, HAL_MAX_DELAY);
	//check if u received or not
	if(HAL_Status != HAL_OK)
	{
		Status = BL_NACK;
	}
	else
	{
		//the lenght is in the first bit
		DataLength = BL_Host_Buffer[0];
	
		//after that get all bytes depending on length
		HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_UART, &BL_Host_Buffer[1], DataLength, HAL_MAX_DELAY);
		if(HAL_Status != HAL_OK){
			Status = BL_NACK;
		}
		else
		{
			switch(BL_Host_Buffer[1])
			{
				case CBL_GET_VER_CMD:
					Bootloader_Get_Version(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_GET_HELP_CMD:
					Bootloader_Get_Help(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_GET_CID_CMD:
					Bootloader_Get_Chip_Identification_Number(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_GET_RDP_STATUS_CMD:
					Bootloader_Read_Protection_Level(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_GO_TO_ADDR_CMD:
					Bootloader_Jump_To_Address(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_FLASH_ERASE_CMD:
					Bootloader_Erase_Flash(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				case CBL_MEM_WRITE_CMD:
					Bootloader_Memory_Write(BL_Host_Buffer);
					Status = BL_ACK;
					break;
				default:
					BootLoader_Print_Message("Invalid command code received from host !! \r\n");
					break;
			}
		}
	}
	return Status;
}



void BootLoader_Print_Message(char *format, ...)
{
	char message[100] = {0};
	va_list List;
	//Enables access to the variable arguments
	va_start(List,format);
	//Write formatted data from variable argument list to string 
	vsprintf(message,format,List);
	#if(BL_DEBUG_METHOD == BL_ENABLE_UART_DEBUG_MESSAGE) 
	//Trasmit the formatted data through the defined UART
	HAL_UART_Transmit(&huart2, (uint8_t *)message, sizeof(message), HAL_MAX_DELAY);
	#endif
	
	#if(BL_DEBUG_METHOD == BL_ENABLE_SPI_DEBUG_MESSAGE) 
	//Trasmit the formatted data through the defined SPI
	#endif
	
	#if(BL_DEBUG_METHOD == BL_ENABLE_CAN_DEBUG_MESSAGE) 
	//Trasmit the formatted data through the defined CAN
	#endif
	//Performs cleanup for an ap object
	va_end(List);
}

static void Bootloader_Get_Version(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	uint8_t BL_Version[4] = { CBL_VENDOR_ID, CBL_SW_MAJOR_VERSION, CBL_SW_MINOR_VERSION, CBL_SW_PATCH_VERSION };
	
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Read the bootloader version from the MCU \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify(&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		Bootloader_Send_ACK(4);
		Bootloader_Send_Data_To_Host((uint8_t *)(&BL_Version[0]),4);//BL_Version also is corect
		
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
    BootLoader_Print_Message("Bootloader Ver. %d.%d.%d \r\n", BL_Version[1], BL_Version[2], BL_Version[3]);
		//bootloader_jump_to_user_app(); for testing
#endif  
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		Bootloader_Send_NACK();
	}
	
}
static void Bootloader_Get_Help(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Read the commands supported by this bootloader \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		Bootloader_Send_ACK(4);
		Bootloader_Send_Data_To_Host((uint8_t *)(&Bootloader_Supported_Commands[0]),12);
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
}
static void Bootloader_Get_Chip_Identification_Number(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	uint16_t MCU_Device_ID = 0;
	
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Read MCU ID \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		
		//get MCU chip ID Number
		MCU_Device_ID = ((uint16_t)((DBGMCU->IDCODE)& 0x00000FFF));
		Bootloader_Send_ACK(2);
		Bootloader_Send_Data_To_Host((uint8_t *)&MCU_Device_ID,2);
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
	
}
static void Bootloader_Read_Protection_Level(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	uint8_t Protection_Level = 0;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Read FLASH Read Protection Out level \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		Bootloader_Send_ACK(1);
		
		Protection_Level = CBL_STM32F407_Get_RDP_Level();
		
		Bootloader_Send_Data_To_Host((uint8_t *)&Protection_Level, 1);
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
}
static void Bootloader_Jump_To_Address(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
  uint32_t Host_CRC = 0;
	uint32_t Jump_Address = 0;
	uint8_t Address_Verification = ADDRESS_IS_INVALID;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Jump bootloader to specified address \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		
		Bootloader_Send_ACK(1);
		//extract Address From HOST Packet
		Jump_Address = *((uint32_t *)&Host_Buffer[2]);
		
		//Verify the Extracted address
		Address_Verification = Host_Address_Verification(Jump_Address);
		if(ADDRESS_IS_VALID == Address_Verification)
		{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
			BootLoader_Print_Message("Address verification succeeded \r\n");
#endif
			Bootloader_Send_Data_To_Host((uint8_t *)&Address_Verification, 1);
			//Get jumping ADDRESS_IS_INVALID and add 1 for Tbit
			Jump_Ptr JumpAddress = (Jump_Ptr)(Jump_Address + 1);
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
			BootLoader_Print_Message("Jump to : 0x%X \r\n", Jump_Address);
#endif
			JumpAddress();
		}
		else
		{
			// Report address verification failed
			Bootloader_Send_Data_To_Host((uint8_t *)&Address_Verification, 1);
		}
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
	
}
static void Bootloader_Erase_Flash(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	uint8_t Erase_Status = 0;
	
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("erase or sector erase of the user flash \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		Bootloader_Send_ACK(1);
		//perform the erase
		Erase_Status = Perform_Flash_Erase(Host_Buffer[2],Host_Buffer[3]);
		
		if(SUCCESSFUL_ERASE == Erase_Status)
		{
			Bootloader_Send_Data_To_Host((uint8_t *)&Erase_Status, 1);
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
			BootLoader_Print_Message("Successful Erase \r\n");
#endif
		}
		else
		{
			Bootloader_Send_Data_To_Host((uint8_t *)&Erase_Status, 1);
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
			BootLoader_Print_Message("Erase request failed !!\r\n");
#endif
		}
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}
static void Bootloader_Memory_Write(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
  uint32_t Host_CRC = 0;
	uint32_t HOST_Address = 0;
	uint8_t Payload_Len = 0;
	uint8_t Address_Verification = ADDRESS_IS_INVALID;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Write data into different sections of the MCU \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		
		Bootloader_Send_ACK(1);
		
		//extracting Payload and Address need to Write on
		HOST_Address = *((uint32_t *)(&Host_Buffer[2]));
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("HOST_Address = 0x%X \r\n", HOST_Address);
#endif
		Payload_Len = Host_Buffer[6];
		
		//check for valid address
		Address_Verification = Host_Address_Verification(HOST_Address);
		if(ADDRESS_IS_VALID == Address_Verification)
		{
			//write data to flash memory in specific address
			Flash_Payload_Write_Status = Flash_Memory_Write_Payload((uint8_t *)&Host_Buffer[7],HOST_Address,Payload_Len);
			if(FLASH_PAYLOAD_WRITE_PASSED == Flash_Payload_Write_Status)
			{
				Bootloader_Send_Data_To_Host((uint8_t *)&Flash_Payload_Write_Status, 1);
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
				BootLoader_Print_Message("Payload Valid \r\n");
#endif
			}
			else
			{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
				BootLoader_Print_Message("Payload InValid \r\n");
#endif
				Bootloader_Send_Data_To_Host((uint8_t *)&Flash_Payload_Write_Status, 1);
			}
		}
		else
		{
			Address_Verification = ADDRESS_IS_INVALID;
			Bootloader_Send_Data_To_Host((uint8_t *)&Address_Verification, 1);
		}
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
}

static void Bootloader_Change_Read_Protection_Level(uint8_t *Host_Buffer)
{
	uint16_t HostPacket_Len = 0;
	uint32_t Host_CRC = 0;
	uint8_t ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
	uint8_t Host_ROP_Level = 0;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Read FLASH Read Protection Out level \r\n");
#endif
	
	//extract length of packet and CRC
	HostPacket_Len = Host_Buffer[0] + 1;
	Host_CRC = *((uint32_t *)((Host_Buffer + HostPacket_Len) - 4));
	
	//CRC calculation on received data
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] ,HostPacket_Len - 4 ,Host_CRC))
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("CRC Verification Passed \r\n");
#endif
		Bootloader_Send_ACK(1);
		
		//get the protection level
		Host_ROP_Level = Host_Buffer[2];
		
		if((CBL_ROP_LEVEL_2 == Host_ROP_Level)||(OB_RDP_LEVEL_2 == Host_ROP_Level))
		{
			ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
		}
		else
		{
			if(CBL_ROP_LEVEL_0 == Host_ROP_Level)
			{
				Host_ROP_Level = 0xAA; 
			}
			else if(CBL_ROP_LEVEL_1 == Host_ROP_Level)
			{
				Host_ROP_Level = 0x55; 
			}
			ROP_Level_Status = Change_ROP_Level(Host_ROP_Level);
		}
		Bootloader_Send_Data_To_Host((uint8_t *)&ROP_Level_Status, 1);
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("CRC Verification Failed \r\n");
#endif		
		
		Bootloader_Send_NACK();
	}
}

static uint8_t Bootloader_CRC_Verify(uint8_t *pData, uint32_t Data_Len, uint32_t Host_CRC)
{
	uint8_t CRC_Status = CRC_VERIFICATION_FAILED;
	uint32_t CRC_Calculated = 0;
	uint32_t Data_Buffer = 0;
	
	//calculate CRC
	for(uint8_t Counter = 0; Counter < Data_Len; Counter++){
		Data_Buffer = (uint32_t)pData[Counter];
		CRC_Calculated = HAL_CRC_Accumulate(CRC_ENGINE_OBJ, &Data_Buffer, 1);
	}
	
	__HAL_CRC_DR_RESET(CRC_ENGINE_OBJ);
	
	if(CRC_Calculated == Host_CRC)
	{
		CRC_Status = CRC_VERIFICATION_PASSED;
	}
	else
	{
		CRC_Status = CRC_VERIFICATION_FAILED;
	}
	
	return CRC_Status;
}

static void Bootloader_Send_ACK(uint8_t Replay_Len)
{
	//will send 2bytes ACK and LENGTH
	uint8_t Ack_Value[2] = {0};
	
	Ack_Value[0] = CBL_SEND_ACK;
	Ack_Value[1] = Replay_Len;
	
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_UART, (uint8_t *)Ack_Value, 2, HAL_MAX_DELAY);
}
static void Bootloader_Send_NACK(void)
{
	//will send 1byte the NACK
	uint8_t Ack_Value = CBL_SEND_NACK;
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_UART, &Ack_Value, 1, HAL_MAX_DELAY);

}

static void Bootloader_Send_Data_To_Host(uint8_t *Host_Buffer, uint32_t Data_Len)
{
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_UART, Host_Buffer, Data_Len, HAL_MAX_DELAY);
}
static uint8_t Host_Address_Verification(uint32_t Jump_Address)
{
	uint8_t Address_Verification = ADDRESS_IS_INVALID;
	if((Jump_Address >= SRAM1_BASE) && (Jump_Address <= STM32F407XX_SRAM1_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else if((Jump_Address >= SRAM2_BASE) && (Jump_Address <= STM32F407XX_SRAM2_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else if((Jump_Address >= CCMDATARAM_BASE) && (Jump_Address <= STM32F407XX_SRAM3_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else if((Jump_Address >= FLASH_BASE) && (Jump_Address <= STM32F407XX_FLASH_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else
	{
		Address_Verification = ADDRESS_IS_INVALID;
	}
	return Address_Verification;
}
static uint8_t Perform_Flash_Erase(uint8_t SectorNumber, uint8_t NumberOfSectors)
{
	uint8_t Sector_Status = INVALID_SECTOR_NUMBER;
	FLASH_EraseInitTypeDef Erase;
	uint8_t Remaining_Sectors = 0;
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint32_t SectorError = 0;
	
	if(NumberOfSectors > CBL_FLASH_MAX_SECTOR_NUMBER){
		/* Number Of sectors is out of range */
		Sector_Status = INVALID_SECTOR_NUMBER;
	}
	else
	{
		//erase from between
		if((NumberOfSectors <= (CBL_FLASH_MAX_SECTOR_NUMBER - 1))||(CBL_FLASH_MASS_ERASE == SectorNumber))
		{
			if(CBL_FLASH_MASS_ERASE == SectorNumber)
			{
				Erase.TypeErase = FLASH_TYPEERASE_MASSERASE;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
				BootLoader_Print_Message("Flash Mass erase activation \r\n");
#endif
			}
			else
			{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
				BootLoader_Print_Message("User needs Sector erase \r\n");
#endif
				//need to erase from sector 5 20 sectors but the avalibale will be from 5 to 11 (6)
				Remaining_Sectors = CBL_FLASH_MAX_SECTOR_NUMBER - SectorNumber;//12-5
				if(NumberOfSectors > Remaining_Sectors)//is this number grater that remaining
				{
					//force write the remaing sector 
					NumberOfSectors = Remaining_Sectors;
				}
				else{/*nothig*/}
				Erase.TypeErase = FLASH_TYPEERASE_SECTORS;
				
				//this sector is errased
				Erase.Sector = SectorNumber;
				
				//Number of sectors to be erased
				Erase.NbSectors = NumberOfSectors;
			}
			Erase.Banks = FLASH_BANK_1;
			Erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
			
			//unlock FCRegister
			HAL_Status = HAL_FLASH_Unlock();
			
			//perform the Erasing
			HAL_Status = HAL_FLASHEx_Erase(&Erase,&SectorError);
			if(HAL_SUCCESSFUL_ERASE == SectorError)
			{
				Sector_Status = SUCCESSFUL_ERASE;
			}
			else
			{
				Sector_Status = UNSUCCESSFUL_ERASE;
			}
			//lock FCRegister
			HAL_Status = HAL_FLASH_Lock();
		}
		else
		{
			Sector_Status = UNSUCCESSFUL_ERASE;
		}
	}
	
	return Sector_Status;
	
}
static uint8_t Flash_Memory_Write_Payload(uint8_t *Host_Payload, uint32_t Payload_Start_Address, uint16_t Payload_Len)
{
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	uint16_t Counter = 0;
	
	//Unlock FCRegister
	HAL_Status = HAL_FLASH_Unlock();
	
	if(HAL_Status != HAL_OK){
		Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	}
	else
	{
		for(Counter =0;Counter<Payload_Len;Counter++)
		{
			HAL_Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,Payload_Start_Address+Counter,Host_Payload[Counter]);
			if(HAL_Status != HAL_OK)
			{
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
				break;
			}
			else
			{
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;
			}
			
		}
	}
	if((FLASH_PAYLOAD_WRITE_PASSED == Flash_Payload_Write_Status)&&(HAL_OK == HAL_Status))
	{
		HAL_Status = HAL_FLASH_Lock();
		if(HAL_Status != HAL_OK)
		{
		Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
		}
		else
		{
			Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;
		}
	}
	else
	{
		Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	}
	return Flash_Payload_Write_Status;
}

static uint8_t CBL_STM32F407_Get_RDP_Level()
{
	FLASH_OBProgramInitTypeDef FLASH_OBProgram;
	
	HAL_FLASHEx_OBGetConfig(&FLASH_OBProgram);
	
	return (uint8_t)(FLASH_OBProgram.RDPLevel);
}
static uint8_t Change_ROP_Level(uint32_t ROP_Level)
{
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	FLASH_OBProgramInitTypeDef FLASH_OBProgramInit;
	uint8_t ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
	
	HAL_Status = HAL_FLASH_OB_Unlock();
	
	if(HAL_Status != HAL_OK)
	{
		ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Failed to Unlock the FLASH Control Registers\r\n");
#endif
	}
	else
	{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
		BootLoader_Print_Message("Passed to Unlock the FLASH Control Registers access \r\n");
#endif
		FLASH_OBProgramInit.OptionType = OPTIONBYTE_RDP;
		FLASH_OBProgramInit.Banks = FLASH_BANK_1;
		FLASH_OBProgramInit.RDPLevel = ROP_Level;
		
		
		HAL_Status = HAL_FLASHEx_OBProgram(&FLASH_OBProgramInit);
		if(HAL_Status != HAL_OK)
		{
		ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Failed to Program option bytes\r\n");
#endif
			
			HAL_Status = HAL_FLASH_OB_Lock();
			ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
		}
		else
		{
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
	BootLoader_Print_Message("Passed to Program option bytes\r\n");
#endif
			HAL_Status = HAL_FLASH_OB_Launch();
			if(HAL_Status != HAL_OK)
			{
				ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
			}
			else
			{
				HAL_Status = HAL_FLASH_OB_Lock();
				if(HAL_Status != HAL_OK)
				{
					ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
				}
				else
				{
					ROP_Level_Status = ROP_LEVEL_CHANGE_VALID;
#if (BL_DEBUG_ENABLE == DEBUG_INFO_ENABLE)
					BootLoader_Print_Message("Passed to Program ROP to Level : 0x%X \r\n", ROP_Level);
#endif
				}
			}	
		}
	}
	return ROP_Level_Status;
}

static void bootloader_jump_to_user_app(void)
{
		uint32_t MSP_Value,MainAppAddr;
		//Value of the main stack pointer of our main application
		MSP_Value = *((volatile uint32_t *)FLASH_SECTOR2_BASE_ADDRESS);
	
		//Reset Handler definition function of our main application
		MainAppAddr = *((volatile uint32_t *)(FLASH_SECTOR2_BASE_ADDRESS + 4));
			
		//fetch reset handler
		MainApp ResetHandler_Address = (MainApp)MainAppAddr;
		
		//set MSP
		__set_MSP(MSP_Value);
		
		//Disable all modules
		HAL_RCC_DeInit();
		
		//jump to resetHandler to se if going to main or sysinit
		ResetHandler_Address();
}