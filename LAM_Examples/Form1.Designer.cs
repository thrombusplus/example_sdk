namespace LAM_Examples
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            button_status = new Button();
            button_start_imu = new Button();
            button_stop_imu = new Button();
            pictureBox1 = new PictureBox();
            samplingRateCombo = new ComboBox();
            label1 = new Label();
            textbox_status = new TextBox();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // button_status
            // 
            button_status.Location = new Point(14, 12);
            button_status.Name = "button_status";
            button_status.Size = new Size(75, 23);
            button_status.TabIndex = 0;
            button_status.Text = "Get Status";
            button_status.UseVisualStyleBackColor = true;
            button_status.Click += button_getStatus_Click;
            // 
            // button_start_imu
            // 
            button_start_imu.Location = new Point(14, 135);
            button_start_imu.Name = "button_start_imu";
            button_start_imu.Size = new Size(88, 23);
            button_start_imu.TabIndex = 1;
            button_start_imu.Text = "Start IMU";
            button_start_imu.UseVisualStyleBackColor = true;
            button_start_imu.Click += button_start_imu_Click;
            // 
            // button_stop_imu
            // 
            button_stop_imu.Location = new Point(145, 135);
            button_stop_imu.Name = "button_stop_imu";
            button_stop_imu.Size = new Size(90, 23);
            button_stop_imu.TabIndex = 2;
            button_stop_imu.Text = "Stop IMU";
            button_stop_imu.UseVisualStyleBackColor = true;
            button_stop_imu.Click += button_stop_imu_Click;
            // 
            // pictureBox1
            // 
            pictureBox1.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            pictureBox1.Image = (Image)resources.GetObject("pictureBox1.Image");
            pictureBox1.Location = new Point(281, 12);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(507, 426);
            pictureBox1.SizeMode = PictureBoxSizeMode.CenterImage;
            pictureBox1.TabIndex = 8;
            pictureBox1.TabStop = false;
            pictureBox1.Paint += pictureBox1_Paint;
            // 
            // samplingRateCombo
            // 
            samplingRateCombo.FormattingEnabled = true;
            samplingRateCombo.Items.AddRange(new object[] { "5", "15", "30", "45", "60" });
            samplingRateCombo.Location = new Point(163, 96);
            samplingRateCombo.Name = "samplingRateCombo";
            samplingRateCombo.Size = new Size(72, 23);
            samplingRateCombo.TabIndex = 9;
            samplingRateCombo.Text = "15";
            samplingRateCombo.SelectedValueChanged += samplingRateCombo_ValueChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(14, 96);
            label1.Name = "label1";
            label1.Size = new Size(108, 15);
            label1.TabIndex = 10;
            label1.Text = "Sampling Rate (Hz)";
            // 
            // textbox_status
            // 
            textbox_status.Location = new Point(14, 41);
            textbox_status.Name = "textbox_status";
            textbox_status.ReadOnly = true;
            textbox_status.Size = new Size(261, 23);
            textbox_status.TabIndex = 11;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(textbox_status);
            Controls.Add(label1);
            Controls.Add(samplingRateCombo);
            Controls.Add(pictureBox1);
            Controls.Add(button_stop_imu);
            Controls.Add(button_start_imu);
            Controls.Add(button_status);
            Name = "Form1";
            Text = "Form1";
            Load += Form1_Load;
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button button_status;
        private Button button_start_imu;
        private Button button_stop_imu;
        private PictureBox pictureBox1;
        private ComboBox samplingRateCombo;
        private Label label1;
        private TextBox textbox_status;
    }
}
