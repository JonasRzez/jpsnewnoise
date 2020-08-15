library(ggplot2)
library(latex2exp)

setwd("~/Documents/phd/c++/jpscorenew/jpscore/files/jureca_upload")
path=read.csv("path.csv",header = TRUE,sep=',')$path

exp_data = read.csv("exp_results/NT.csv",header = TRUE, sep = ",")
sim_data = read.csv(paste(path,"flow/NT.csv",sep = ""),header = TRUE, sep = ",")

print(sim_data)
p_sim =  ggplot(sim_data,aes(t,NT, color = as.character(motivation), group = num))  + geom_line(size = .5, alpha = 0.2) +
  geom_line(data = exp_data, mapping = aes(x = t,y = NT, color = as.character(motivation), group = num), size = 1,alpha = 1) + scale_color_manual(breaks = c("high", "low", "0.1","1.3"),values=c("red", "blue", "#FA4848","#4848FA"),name = "motivation/T") +
  ylab(TeX("N(t)")) + xlab(TeX("t in s")) +  theme_classic(base_size = 17)   + stat_summary(data = exp_data, fun=mean,geom="line",lwd=2,aes(group=1))
ggsave(paste(path,"plots/NT.pdf", sep = ""))
print(p_sim)

