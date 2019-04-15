import subprocess
import sys
import time


if __name__== "__main__":
	num_replicas = int(sys.argv[1])
	server_addresses = []
	for i in range(num_replicas):
		server_addresses.append(("0.0.0.0:"+ str(8000+i), "0.0.0.0:"+ str(9000+i)))
	for i in range(num_replicas):
		argv = ["./server"]
		argv.append(server_addresses[i][0])
		argv.append(server_addresses[i][1])
		for j in range(num_replicas):
			if j != i:
				argv.append(server_addresses[j][1])
		print("Starting replica: ", i)
		subprocess.Popen(argv)
		time.sleep(2)
	while True:
		time.sleep(2)