he-handout-v1.tgz:
	tar -zcvf he-handout-v1.tgz doc handout

clean:
	rm -f he-handout-v1.tgz


d1_assert_client:
	echo "\n------ASSERTING------\n"
	./servers/linux-intel-ubuntu-22.04/d1_server > d1_server_actual_output

d1_assert_client:
	echo "\n------ASSERTING------\n"
	sleep 7 && ./handout/d1_test_client localhost 2024 > d1_client_output &
	./servers/linux-intel-ubuntu-22.04/d1_server 2024 > d1_server_actual_output
