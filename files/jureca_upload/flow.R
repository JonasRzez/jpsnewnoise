library(ggplot2)
library(latex2exp)
setwd("~/Documents/phd/c++/jpscorenew/jpscore/files/jureca_upload")
path=read.csv("path.csv",header = TRUE,sep=',')$path
exp_data = read.csv("exp_results/DT.csv",header = TRUE, sep = ",")
sim_data = read.csv(paste(path,"flow/flow_err.csv",sep = ""),header = TRUE, sep = ",")
DT_data_01 = read.csv(paste(path,"flow/DThist0.1.csv",sep = ""),header = TRUE, sep = ",")
DT_data_13 = read.csv(paste(path,"flow/DThist1.3.csv",sep = ""),header = TRUE, sep = ",")
DT_exp = read.csv("exp_results/DThist.csv",header = TRUE, sep = ",")
print(head(DT_data))


p_sim =  ggplot(sim_data,aes(b,DT, color = as.character(T))) + geom_point(size = 4) + scale_color_discrete(name = "Motivation/T")  + 
  geom_errorbar(aes(ymin=DT-DTerr_down, ymax=DT + DTerr_up)) +  theme(text = element_text(size=70)) + theme_classic(base_size = 17) + geom_point(data = exp_data, mapping = aes(x = b,y = DT, color = mot), size = 4) +
  ylab(TeX("$\\Delta T$ in s")) + xlab(TeX("$b$ in m"))
ggsave(paste(path,"plots/flow.pdf", sep = ""))
print(p_sim )

print(ggplot(DT_data_01,aes(x = DT)) + geom_histogram(bins = 80) + scale_y_log10())#+ geom_histogram(data = DT_data_13, aes(x = DT), bins = 80))
print(ggplot(DT_data_13,aes(x = DT)) + geom_histogram(bins = 80) + scale_y_log10())
print(ggplot(DT_exp,aes(x = DT, color = motivation)) + geom_histogram(aes(y = stat(count / sum(count))),bins = 30) + scale_y_log10()) 
print(ggplot(DT_exp, aes(x=DT,color = motivation)) + geom_histogram(aes(y = stat(count / sum(count))),bins = 80)  + theme_classic( base_size = 14))  