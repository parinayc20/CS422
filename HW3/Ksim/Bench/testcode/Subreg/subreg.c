int main(void)
{
   char num[] = {0x12, 0x34, 0x12, 0x34, 0x56, 0x78, 0x56, 0x78};
   int res_num = 0x34123456;
   int final_num = 0;
   memcpy(&final_num, num + 1, sizeof(int));
   printf("final_num = %d\n", final_num);
   printf("res_num = %d\n", res_num);
   if (final_num == res_num)
	printf("Test successful\n");
   else
	printf("Test failed\n");
   return 0;
}

