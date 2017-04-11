local mg     = require "moongen"
local memory = require "memory"
local device = require "device"
local ts     = require "timestamping"
local filter = require "filter"
local hist   = require "histogram"
local stats  = require "stats"
local timer  = require "timer"
local log    = require "log"
local pcap   = require "pcap"

function configure(parser)
	parser:description("Generates UDP traffic based on a pcap file."..
     "Searches for the maximum rate that still passes through with <1% loss.")
	parser:argument("txDev", "Device to transmit from."):convert(tonumber)
	parser:argument("rxDev", "Device to receive from."):convert(tonumber)
  parser:argument("file", "File to replay."):args(1)
	parser:option("-r --rate", "Transmit rate in Mbit/s."):default(10000):convert(tonumber)
	parser:option("-f --flows", "Number of flows (randomized source IP)."):default(4):convert(tonumber)
	parser:option("-s --size", "Packet size."):default(60):convert(tonumber)
	parser:option("-u --upheat", "Heatup time before beginning of latency measurement"):default(2):convert(tonumber)
	parser:option("-t --timeout", "Time to run the test"):default(0):convert(tonumber)
end

function setRate(queue, packetSize, rate_mbps)
	queue:setRate(rate_mbps - (packetSize + 4) * 8 / 1000);
end

function master(args)
	txDev = device.config{port = args.txDev, rxQueues = 3, txQueues = 3}
	rxDev = device.config{port = args.rxDev, rxQueues = 3, txQueues = 3}
	device.waitForLinks()
	local file = io.open("pcap-find-1p-results.txt", "w")
	file:write("rate #pkt #pkt/s loss\n")
	local maxRate = args.rate
	if maxRate <= 0 then
		maxRate = 10000
	end
	local minRate = 10
	local rate = minRate + (maxRate - minRate)/2
  -- Heatup phase
  printf("heatup at %d rate - %d secs", minRate, args.upheat);
  setRate(txDev:getTxQueue(0), args.size, minRate);
  local loadTask = mg.startTask("loadSlave", txDev:getTxQueue(0), rxDev,
                                args.upheat, args.file)
  local snt, rcv = loadTask:wait()
  printf("heatup results: %d sent, %f loss", snt, (snt-rcv)/snt);
  if (rcv < snt/100) then
    printf("unsuccessfull exiting");
    return
  end
  mg.waitForTasks()
  local steps = 11;
  local upperbound = maxRate
  local lowerbound = minRate
  -- Testing phase
  for i = 1, steps  do
    printf("running step %d/%d , %d Mbps", i, steps, rate);
    setRate(txDev:getTxQueue(0), args.size, rate);
    local packetsSent
    local packetsRecv
    local loadTask = mg.startTask("loadSlave", txDev:getTxQueue(0), rxDev,
                                  args.timeout, args.file)
    packetsSent, packetsRecv = loadTask:wait()
    local loss = (packetsSent - packetsRecv)/packetsSent
    printf("total: %d rate, %d sent, %f lost",
           rate, packetsSent, loss);
    mg.waitForTasks()
    if (i+1 == steps) then
      file:write(rate .. " " .. packetsSent .. " " ..
                   packetsSent/args.timeout .. " " .. loss .. "\n")
    end
    if (loss < 0.01) then
      lowerbound = rate
      rate = rate + (upperbound - rate)/2
    else
      upperbound = rate
      rate = lowerbound + (rate - lowerbound)/2
    end
	end
end

function loadSlave(queue, rxDev, duration, fname)
	local mempool = memory.createMemPool()
	local bufs = mempool:bufArray()
	local finished = timer:new(duration)
	local fileTxCtr = stats:newDevTxCounter("txpkts", queue, "CSV", "txpkts.csv")
	local fileRxCtr = stats:newDevRxCounter("rxpkts", rxDev, "CSV", "rxpkts.csv")
	local txCtr = stats:newDevTxCounter(" tx", queue, "nil")
	local rxCtr = stats:newDevRxCounter(" rx", rxDev, "nil")
  local pcapFile = pcap:newReader(fname)
	while finished:running() and mg.running() do
    local n = pcapFile:read(bufs)
    if (n == 0) then
      pcapFile:reset()
    end
	--	bufs:offloadUdpChecksums()
    queue:sendN(bufs, n)
		txCtr:update()
		fileTxCtr:update()
		rxCtr:update()
		fileRxCtr:update()
	end
	txCtr:finalize()
	fileTxCtr:finalize()
	rxCtr:finalize()
	fileRxCtr:finalize()
	pcapFile:close()
	return txCtr.total, rxCtr.total
end
