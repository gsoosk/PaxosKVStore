import subprocess
import sys
import time

"my_addr: '0.0.0.0:8000' my_paxos: '0.0.0.0:9000' fail_rate: 0.3 replica: '0.0.0.0:9000' replica: '0.0.0.0:9001' replica: '0.0.0.0:9002' replica: '0.0.0.0:9003' replica: '0.0.0.0:9004'"
if __name__== "__main__":
	num_replicas = int(sys.argv[1])
	fail_rate = float(sys.argv[2])
	server_configs = []
	server_addresses = []
	for i in range(num_replicas):
		server_addresses.append(("0.0.0.0:"+ str(8000+i), "0.0.0.0:"+ str(9000+i)))
	for i in range(num_replicas):
		argv = ["./server"]
		server_config = "my_addr:'"+server_addresses[i][0]+"' my_paxos:'"+server_addresses[i][1]+"' fail_rate:"+str(fail_rate)
		for j in range(num_replicas):
			server_config = server_config+" replica: '"+server_addresses[j][1]+"'"
		argv.append(server_config)
		print("Starting replica: ", i)
		subprocess.Popen(argv)
		time.sleep(2)
	while True:
		time.sleep(2)