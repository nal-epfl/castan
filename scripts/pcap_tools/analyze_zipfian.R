library(stats4)

args <- commandArgs(trailingOnly=TRUE)
print(args[1])

fr <- read.table(args[1],header=TRUE)
fr <- fr$n_pkts
fr <- sort(fr,decreasing=TRUE)
p <- fr/sum(fr)
lzipf <- function(s, N) -s*log(1:N)-log(sum(1/(1:N)^s))
opt.f <- function(s) sum((log(p)-lzipf(s,length(p)))^2)
opt <- optimize(opt.f, c(0.5,10))

print("optimizer")
print(opt)

ll <- function(s) sum(fr*(s*log(1:length(fr))+log(sum(1/(1:length(fr))^s))))
fit <- mle(ll,start=list(s=1))

print("minimum likelihood")
print(summary(fit))
s.sq <- opt$minimum
s.ll <- coef(fit)

png("zipf_plot.png")
plot(1:length(fr), p, log="xy")
lines(1:length(fr),exp(lzipf(s.sq, length(fr))), col=2)
lines(1:length(fr),exp(lzipf(s.ll, length(fr))), col=3)
dev.off()
