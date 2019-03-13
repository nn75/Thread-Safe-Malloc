echo "===================Making===================="
make clean
make
for i in {1..20}
         do
# echo "====================Testing thread_test===================="
# ./thread_test
# echo "====================Testing thread_test_malloc_free===================="
# ./thread_test_malloc_free
# echo "====================$i Testing thread_test_malloc_free_change_thread===================="
# ./thread_test_malloc_free_change_thread #> tmp.txt
 echo "====================$i thread_test_measurement===================="
 ./thread_test_measurement
done
echo "===================Cleaning===================="
make clean
