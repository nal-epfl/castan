vagrant up
vagrant ssh router -c 'sudo bash /nf/testbed/redeploy-nf.sh'
vagrant ssh router -c 'sudo bash /nf/testbed/run-nf.sh' &
sleep 3
./test-nf.sh
